using System.Runtime.InteropServices;

namespace ValoAI.App.Helpers;

/// <summary>
/// Lightweight wrapper around IMMDeviceEnumerator via COM interop.
/// Enumerates active audio capture (mic) endpoints without any third-party lib.
/// </summary>
public class MMDeviceEnumeratorWrapper
{
    [Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    interface IMMDeviceEnumerator
    {
        void EnumAudioEndpoints(int dataFlow, int stateMask,
            out IMMDeviceCollection devices);
        void GetDefaultAudioEndpoint(int dataFlow, int role,
            out IMMDevice device);
        void GetDevice(string id, out IMMDevice device);
        void RegisterEndpointNotificationCallback(IntPtr client);
        void UnregisterEndpointNotificationCallback(IntPtr client);
    }

    [Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    interface IMMDeviceCollection
    {
        void GetCount(out uint count);
        void Item(uint index, out IMMDevice device);
    }

    [Guid("D666063F-1587-4E43-81F1-B948E807363F")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    interface IMMDevice
    {
        void Activate(ref Guid iid, int clsCtx, IntPtr activationParams,
            out IntPtr iface);
        void OpenPropertyStore(int access, out IPropertyStore store);
        void GetId(out string id);
        void GetState(out int state);
    }

    [Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    interface IPropertyStore
    {
        void GetCount(out uint count);
        void GetAt(uint prop, out PROPERTYKEY key);
        void GetValue(ref PROPERTYKEY key, out PROPVARIANT val);
        void SetValue(ref PROPERTYKEY key, ref PROPVARIANT val);
        void Commit();
    }

    [StructLayout(LayoutKind.Sequential)]
    struct PROPERTYKEY
    {
        public Guid fmtid;
        public uint pid;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct PROPVARIANT
    {
        public ushort vt;
        public ushort r1, r2, r3;
        public IntPtr ptr;
    }

    // PKEY_Device_FriendlyName
    static readonly PROPERTYKEY PKEY_FriendlyName = new()
    {
        fmtid = new Guid("a45c254e-df1c-4efd-8020-67d146a850e0"),
        pid = 14
    };

    [DllImport("ole32.dll")]
    static extern int CoCreateInstance(
        [MarshalAs(UnmanagedType.LPStruct)] Guid rclsid,
        IntPtr pUnkOuter, uint dwClsContext,
        [MarshalAs(UnmanagedType.LPStruct)] Guid riid,
        out IMMDeviceEnumerator ppv);

    public List<(string Id, string Name)> GetCaptureDevices()
    {
        var result = new List<(string, string)>();

        var clsid = new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E");
        var iid = new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E");

        // MMDeviceEnumerator CLSID
        var enumClsid = new Guid("BCDe0395-e52f-467c-8e3d-c4579291692e");
        // Correct CLSID for MMDeviceEnumerator
        var mmClsid = new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E");

        int hr = CoCreateInstance(
            new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E"),
            IntPtr.Zero, 1,
            new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E"),
            out var enumerator);

        if (hr != 0) return result;

        enumerator.EnumAudioEndpoints(1 /*eCapture*/, 1 /*DEVICE_STATE_ACTIVE*/,
            out var collection);

        collection.GetCount(out uint count);

        for (uint i = 0; i < count; i++)
        {
            try
            {
                collection.Item(i, out var device);
                device.GetId(out string id);
                device.OpenPropertyStore(0, out var store);

                var key = PKEY_FriendlyName;
                store.GetValue(ref key, out var pv);

                string name = pv.ptr != IntPtr.Zero
                    ? Marshal.PtrToStringUni(pv.ptr) ?? $"Device {i}"
                    : $"Device {i}";

                result.Add((id, name));
            }
            catch { /* skip bad device */ }
        }

        return result;
    }
}