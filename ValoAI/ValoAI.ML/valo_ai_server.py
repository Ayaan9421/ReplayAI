"""
valo_ai_server.py — Valo AI ML Server (Flask + Waitress)
=========================================================
Lightweight ML inference server for the Valo AI pipeline.
Managed by ValoAI.App (C#) via localhost HTTP.

Start:
    python valo_ai_server.py --port 8765

Endpoints:
    GET  /health    -> model status
    POST /analyze   -> run inference on a clip
    POST /shutdown  -> free VRAM and exit

Optimizations over v1:
  - Frame preprocessing (letterbox + normalise) runs on GPU
  - DeltaEncoder runs in one batched call instead of N individual calls
  - Garbage collector runs after each request to reclaim RAM
  - decord CPU context used reliably (GPU context unreliable on Windows pip build)
"""

# HuggingFace cache redirect — MUST be before any torch/transformers imports
import os
import gc
from pathlib import Path

_SCRIPT_DIR = Path(__file__).parent.resolve()
os.environ["HF_HOME"]               = str(_SCRIPT_DIR / "hub")
os.environ["HUGGINGFACE_HUB_CACHE"] = str(_SCRIPT_DIR / "hub")
os.environ["TORCH_HOME"]            = str(_SCRIPT_DIR / "hub" / "torch")
os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

import json
import time
import argparse
import warnings
import threading

warnings.filterwarnings("ignore")

import torch
import torch.nn as nn
import torch.nn.functional as F
import torchvision.transforms.functional as TF
from flask import Flask, request, jsonify

# ── Constants ──────────────────────────────────────────────────────────────
EXTRACT_FPS    = 5
TARGET_SIZE    = 224
DINO_DIM       = 384
NUM_PATCHES    = 256
DELTA_DIM      = 256
QWEN_DIM       = 2048
DINO_CHUNK     = 4     
MODEL_ID       = "Qwen/Qwen2.5-VL-3B-Instruct"

DELTATOK_PATH   = _SCRIPT_DIR / "models" / "deltatok_best.pt"
PROJECTION_PATH = _SCRIPT_DIR / "models" / "projection_best.pt"
LORA_PATH       = _SCRIPT_DIR / "models" / "lora_best"

# ImageNet stats as GPU tensors — created once, reused every request
DEVICE         = torch.device("cuda" if torch.cuda.is_available() else "cpu")
IMAGENET_MEAN  = torch.tensor([0.485, 0.456, 0.406], device=DEVICE).view(1, 3, 1, 1)
IMAGENET_STD   = torch.tensor([0.229, 0.224, 0.225], device=DEVICE).view(1, 3, 1, 1)


# ── Global model state ─────────────────────────────────────────────────────
_state = {
    "ready"     : False,
    "loading"   : False,
    "dino"      : None,
    "encoder"   : None,
    "projection": None,
    "model"     : None,
    "tokenizer" : None,
    "load_error": None,
}


# ── Model architecture ──────────────────────────────────────────────────────

class DeltaEncoder(nn.Module):
    def __init__(self, dino_dim=384, num_patches=256,
                 delta_dim=256, num_heads=8, num_layers=4):
        super().__init__()
        self.input_proj  = nn.Linear(dino_dim * 2, delta_dim)
        self.delta_query = nn.Parameter(torch.randn(1, 1, delta_dim))
        self.pos_embed   = nn.Parameter(torch.randn(1, num_patches, delta_dim))
        encoder_layer    = nn.TransformerEncoderLayer(
            d_model=delta_dim, nhead=num_heads,
            dim_feedforward=delta_dim * 4,
            dropout=0.1, batch_first=True
        )
        self.transformer = nn.TransformerEncoder(encoder_layer,
                                                  num_layers=num_layers)
        self.cross_attn  = nn.MultiheadAttention(
            embed_dim=delta_dim, num_heads=num_heads,
            dropout=0.1, batch_first=True
        )
        self.norm = nn.LayerNorm(delta_dim)

    def forward(self, feat_t, feat_t1):
        # feat_t, feat_t1: [B, N, D] — B pairs processed at once
        B    = feat_t.shape[0]
        x    = torch.cat([feat_t, feat_t1], dim=-1)
        x    = self.input_proj(x) + self.pos_embed
        x    = self.transformer(x)
        q    = self.delta_query.expand(B, -1, -1)
        d, _ = self.cross_attn(q, x, x)
        return self.norm(d)   # [B, 1, delta_dim]


class DeltaProjection(nn.Module):
    def __init__(self, dino_dim=384, delta_dim=256, qwen_dim=2048):
        super().__init__()
        self.anchor_proj = nn.Sequential(
            nn.Linear(dino_dim, qwen_dim),
            nn.GELU(),
            nn.Linear(qwen_dim, qwen_dim),
            nn.LayerNorm(qwen_dim),
        )
        self.delta_proj = nn.Sequential(
            nn.Linear(delta_dim, qwen_dim),
            nn.GELU(),
            nn.Linear(qwen_dim, qwen_dim),
            nn.LayerNorm(qwen_dim),
        )
        self.anchor_type_embed = nn.Parameter(torch.randn(1, 1, qwen_dim) * 0.02)
        self.delta_type_embed  = nn.Parameter(torch.randn(1, 1, qwen_dim) * 0.02)

    def forward(self, anchor_tokens, delta_tokens):
        anchor = self.anchor_proj(anchor_tokens) + self.anchor_type_embed
        delta  = self.delta_proj(delta_tokens)   + self.delta_type_embed
        return torch.cat([anchor, delta], dim=1)


# ── Step 1: Frame extraction ────────────────────────────────────────────────
# Uses decord CPU context (reliable on Windows) — returns raw uint8 numpy
# arrays which get batched and moved to GPU in step 2.

def extract_frames(clip_path: str) -> torch.Tensor:
    """
    Extract frames and return as a single GPU tensor [N, 3, H, W] float32 0-1.
    Decord CPU context is used — GPU preprocessing happens in extract_dino_features.
    """
    raw_frames = []

    try:
        from decord import VideoReader, cpu as dcpu
        vr         = VideoReader(clip_path, ctx=dcpu(0))
        native_fps = vr.get_avg_fps()
        step       = max(1, round(native_fps / EXTRACT_FPS))
        indices    = list(range(0, len(vr), step))

        # Batch fetch all frames at once — much faster than one-by-one
        batch = vr.get_batch(indices).asnumpy()   # [N, H, W, 3] uint8
        del vr

        # Move entire batch to GPU at once
        tensor = torch.from_numpy(batch).permute(0, 3, 1, 2).to(DEVICE)  # [N, 3, H, W]
        return tensor.float() / 255.0

    except Exception:
        pass

    # OpenCV fallback
    import cv2
    cap        = cv2.VideoCapture(clip_path)
    native_fps = cap.get(cv2.CAP_PROP_FPS)
    step       = max(1, round(native_fps / EXTRACT_FPS))
    raw_idx    = 0
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        if raw_idx % step == 0:
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            raw_frames.append(torch.from_numpy(frame))
        raw_idx += 1
    cap.release()

    if not raw_frames:
        return None

    # Stack and move to GPU at once
    tensor = torch.stack(raw_frames).permute(0, 3, 1, 2).to(DEVICE)
    return tensor.float() / 255.0


# ── Step 2: GPU preprocessing + DINOv2 features ────────────────────────────

@torch.no_grad()
def preprocess_batch_gpu(frames: torch.Tensor, size: int = 224) -> torch.Tensor:
    """
    Letterbox resize + ImageNet normalise — fully on GPU.
    Input:  [N, 3, H, W] float32 0-1  (already on DEVICE)
    Output: [N, 3, size, size] float32 normalised
    """
    N, C, H, W = frames.shape
    scale       = size / max(H, W)
    new_h       = int(H * scale)
    new_w       = int(W * scale)

    # Batch resize on GPU
    resized = F.interpolate(frames, size=(new_h, new_w),
                            mode="bilinear", align_corners=False)

    # Pad to square
    pad_top    = (size - new_h) // 2
    pad_bottom = size - new_h - pad_top
    pad_left   = (size - new_w) // 2
    pad_right  = size - new_w - pad_left

    padded = F.pad(resized, [pad_left, pad_right, pad_top, pad_bottom], value=0.0)

    # Normalise using pre-allocated GPU tensors
    normalised = (padded - IMAGENET_MEAN) / IMAGENET_STD

    return normalised   # [N, 3, 224, 224]


@torch.no_grad()
def extract_dino_features(frames: torch.Tensor) -> torch.Tensor:
    """
    Preprocess frames on GPU and run DINOv2 in chunks.
    Input:  [N, 3, H, W] on DEVICE
    Output: [N, 256, 384] on CPU (to save VRAM between steps)
    """
    preprocessed = preprocess_batch_gpu(frames, TARGET_SIZE)  # [N, 3, 224, 224] on GPU
    all_feats     = []

    with torch.cuda.amp.autocast(dtype=torch.bfloat16):
        for i in range(0, len(preprocessed), DINO_CHUNK):
            chunk = preprocessed[i : i + DINO_CHUNK]
            feats = _state["dino"].get_intermediate_layers(chunk, n=1)[0]
            all_feats.append(feats.cpu())

    # Free preprocessed frames from VRAM immediately
    del preprocessed
    torch.cuda.empty_cache()

    return torch.cat(all_feats, dim=0)   # [N, 256, 384] on CPU


# ── Step 3: Batched delta compression ──────────────────────────────────────

@torch.no_grad()
def compress_to_tokens(all_feats: torch.Tensor):
    """
    Run DeltaEncoder in ONE batched call instead of N individual calls.
    Eliminates N-1 individual CUDA kernel launches and CPU overhead.

    all_feats: [N, 256, 384] on CPU
    Returns: anchor_tokens [256, 384], delta_tokens [N-1, 256] both on CPU
    """
    anchor_tokens = all_feats[0]   # [256, 384]

    if len(all_feats) < 2:
        return anchor_tokens, torch.zeros(0, DELTA_DIM)

    # Build consecutive pairs as a batch
    feat_t  = all_feats[:-1].to(DEVICE)   # [N-1, 256, 384]
    feat_t1 = all_feats[1:].to(DEVICE)    # [N-1, 256, 384]

    # Single batched forward pass — replaces N-1 individual calls
    delta_tokens = _state["encoder"](feat_t, feat_t1)   # [N-1, 1, 256]
    delta_tokens = delta_tokens.squeeze(1).cpu()         # [N-1, 256]

    # Free pair tensors from VRAM
    del feat_t, feat_t1
    torch.cuda.empty_cache()

    return anchor_tokens, delta_tokens


# ── Step 4: Qwen generation ─────────────────────────────────────────────────

@torch.no_grad()
def run_qwen(anchor_tokens: torch.Tensor,
             delta_tokens: torch.Tensor) -> dict:
    model     = _state["model"]
    tokenizer = _state["tokenizer"]
    proj      = _state["projection"]

    system_prompt = (
        "You are a professional Valorant coach analyzing footage from a player's perspective. "
        "Analyze ONLY the player's team — their decisions, economy, utility, and rotations. "
        "Never analyze or give advice to the enemy team. "
        "Use 'your team' and 'you' in all coaching. "
        "Always respond with a valid JSON object containing exactly these four keys: "
        "round_breakdown, strategic_insight, transferable_principles (array of strings), team_advice. "
        "No markdown, no preamble, no explanation. Only the JSON object."
    )

    messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user",   "content": "Analyze this Valorant round and provide structured coaching."},
    ]

    prompt_text = tokenizer.apply_chat_template(
        messages, tokenize=False, add_generation_prompt=True
    )
    prompt_ids = tokenizer(
        prompt_text, return_tensors="pt"
    )["input_ids"].to(DEVICE)

    anchor_t      = anchor_tokens.unsqueeze(0).to(DEVICE)
    delta_t       = delta_tokens.unsqueeze(0).to(DEVICE)
    visual_embeds = proj(anchor_t, delta_t).to(torch.bfloat16)
    text_embeds   = model.get_input_embeddings()(prompt_ids)
    combined      = torch.cat([visual_embeds, text_embeds], dim=1)

    total_len      = combined.shape[1]
    attention_mask = torch.ones(1, total_len, dtype=torch.long, device=DEVICE)
    position_ids   = torch.arange(total_len, device=DEVICE).unsqueeze(0)

    output_ids = model.generate(
        input_ids      = None,
        inputs_embeds  = combined,
        attention_mask = attention_mask,
        position_ids   = position_ids,
        max_new_tokens = 600,
        temperature    = 0.7,
        do_sample      = True,
        top_p          = 0.9,
        pad_token_id   = tokenizer.eos_token_id,
    )

    raw = tokenizer.decode(output_ids[0], skip_special_tokens=True)

    # Free Qwen intermediate tensors
    del combined, visual_embeds, text_embeds, output_ids
    del anchor_t, delta_t
    torch.cuda.empty_cache()

    try:
        start = raw.find("{")
        end   = raw.rfind("}") + 1
        if start != -1 and end > start:
            return json.loads(raw[start:end])
    except json.JSONDecodeError:
        pass

    return {"raw_output": raw}


# ── Model loading / unloading ───────────────────────────────────────────────

def load_models():
    from transformers import AutoTokenizer, BitsAndBytesConfig
    from transformers import Qwen2_5_VLForConditionalGeneration
    from peft import PeftModel

    _state["loading"]    = True
    _state["load_error"] = None

    try:
        print(f"Device : {DEVICE}")
        if torch.cuda.is_available():
            print(f"GPU    : {torch.cuda.get_device_name(0)}")

        print("\n[1/4] Loading DINOv2-small...")
        dino = torch.hub.load("facebookresearch/dinov2", "dinov2_vits14",
                               trust_repo=True)
        dino = dino.to(DEVICE).eval()
        for p in dino.parameters():
            p.requires_grad = False
        _state["dino"] = dino
        print("  DINOv2 ready.")

        print("\n[2/4] Loading DeltaTok encoder...")
        ckpt   = torch.load(str(DELTATOK_PATH), map_location=DEVICE,
                             weights_only=False)
        config = ckpt.get("config", {})
        encoder = DeltaEncoder(
            dino_dim    = config.get("dino_dim",        DINO_DIM),
            num_patches = config.get("num_patches",     NUM_PATCHES),
            delta_dim   = config.get("delta_token_dim", DELTA_DIM),
        ).to(DEVICE).eval()
        state  = {k.replace("module.", ""): v for k, v in ckpt["encoder"].items()}
        encoder.load_state_dict(state)
        _state["encoder"] = encoder
        print("  DeltaTok ready.")

        print("\n[3/4] Loading projection layer...")
        proj_ckpt  = torch.load(str(PROJECTION_PATH), map_location=DEVICE,
                                 weights_only=False)
        projection = DeltaProjection(
            dino_dim  = DINO_DIM,
            delta_dim = DELTA_DIM,
            qwen_dim  = QWEN_DIM,
        ).to(DEVICE).eval()
        projection.load_state_dict(proj_ckpt["projection"])
        _state["projection"] = projection
        print("  Projection ready.")

        print("\n[4/4] Loading Qwen2.5-VL-3B + LoRA...")
        bnb_config = BitsAndBytesConfig(
            load_in_4bit              = True,
            bnb_4bit_quant_type       = "nf4",
            bnb_4bit_compute_dtype    = torch.bfloat16,
            bnb_4bit_use_double_quant = True,
        )
        tokenizer = AutoTokenizer.from_pretrained(
            str(LORA_PATH), trust_remote_code=True
        )
        tokenizer.pad_token = tokenizer.eos_token

        base_model = Qwen2_5_VLForConditionalGeneration.from_pretrained(
            MODEL_ID,
            quantization_config = bnb_config,
            device_map          = "auto",
#	    max_memory          = {0: "3GiB", "cpu": "10GiB"},
            trust_remote_code   = True,
            torch_dtype         = torch.bfloat16,
        )
        model = PeftModel.from_pretrained(base_model, str(LORA_PATH))
        model.eval()
        _state["model"]     = model
        _state["tokenizer"] = tokenizer
        print("  Qwen ready.")

        _state["ready"]   = True
        _state["loading"] = False
        print("\nAll models loaded. Server ready.")

    except Exception as e:
        _state["load_error"] = str(e)
        _state["loading"]    = False
        _state["ready"]      = False
        print(f"\nModel load failed: {e}")


def unload_models():
    for key in ["dino", "encoder", "projection", "model", "tokenizer"]:
        _state[key] = None
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
    gc.collect()
    _state["ready"]   = False
    _state["loading"] = False
    print("Models unloaded. VRAM freed.")


# ── Flask app ───────────────────────────────────────────────────────────────

app = Flask(__name__)


@app.route("/health", methods=["GET"])
def health():
    if _state["load_error"]:
        status = "error"
    elif _state["ready"]:
        status = "ready"
    elif _state["loading"]:
        status = "loading"
    else:
        status = "unloaded"

    return jsonify({
        "status"    : status,
        "device"    : str(DEVICE),
        "load_error": _state["load_error"],
    })


@app.route("/analyze", methods=["POST"])
def analyze():
    if not _state["ready"]:
        code   = 503
        detail = ("Models are still loading." if _state["loading"]
                  else "Models not loaded.")
        return jsonify({"error": detail}), code

    data      = request.get_json(silent=True) or {}
    clip_path = data.get("clip_path", "").strip().strip('"')

    if not clip_path:
        return jsonify({"error": "clip_path is required"}), 400
    if not os.path.exists(clip_path):
        return jsonify({"error": f"Clip not found: {clip_path}"}), 400

    # Hold references so we can delete them in finally block
    frames        = None
    all_feats     = None
    anchor_tokens = None
    delta_tokens  = None

    try:
        t0 = time.time()

        frames = extract_frames(clip_path)
        if frames is None or (hasattr(frames, '__len__') and len(frames) == 0):
            return jsonify({"error": "No frames extracted from clip."}), 400
        t1 = time.time()

        all_feats = extract_dino_features(frames)
        t2 = time.time()

        anchor_tokens, delta_tokens = compress_to_tokens(all_feats)
        t3 = time.time()

        coaching = run_qwen(anchor_tokens, delta_tokens)
        t4 = time.time()

        return jsonify({
            "success"      : True,
            "elapsed_secs" : round(t4 - t0, 2),
            "token_count"  : NUM_PATCHES + len(delta_tokens),
            "n_frames"     : int(frames.shape[0]),
            "coaching"     : coaching,
            "timings"      : {
                "frame_extraction"  : round(t1 - t0, 2),
                "dino_features"     : round(t2 - t1, 2),
                "delta_compression" : round(t3 - t2, 2),
                "qwen_generation"   : round(t4 - t3, 2),
            },
        })

    except Exception as e:
        return jsonify({"error": f"Inference failed: {str(e)}"}), 500

    finally:
        # Explicitly delete all large tensors and collect garbage
        # This is what brings RAM back to baseline after each request
        del frames, all_feats, anchor_tokens, delta_tokens
        gc.collect()
        if torch.cuda.is_available():
            torch.cuda.empty_cache()


@app.route("/shutdown", methods=["POST"])
def shutdown():
    def _stop():
        time.sleep(0.5)
        unload_models()
        os.kill(os.getpid(), 9)

    threading.Thread(target=_stop, daemon=True).start()
    return jsonify({"message": "Shutting down."})


# ── Entry point ─────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    print("=" * 50)
    print("VALO AI ML SERVER")
    print("=" * 50)

    t_load = time.time()
    load_models()
    print(f"\nModels loaded in {time.time() - t_load:.1f}s")
    print(f"Starting server on port {args.port}...")
    print("=" * 50)

    from waitress import serve
    serve(app, host="127.0.0.1", port=args.port, threads=4)