using System;
using System.Runtime.InteropServices;


//动态加载dll实现

public class dll_loader
{
#if UNITY_EDITOR

    [DllImport("Kernel32")]
    public static extern int LoadLibrary(string lib_name);

    [DllImport("Kernel32")]
    public static extern int FreeLibrary(int lib_handle);

    [DllImport("Kernel32")]
    public static extern int GetProcAddress(int lib_handle, string func_name);

    public static Delegate GetDelegateFromIntPtr(int func_addr, Type t)
    {
        if (func_addr == 0)
            return null;
        else
            return Marshal.GetDelegateForFunctionPointer(new IntPtr(func_addr), t);
    }

    public static Delegate GetFunctionAddress(int lib_handle, string func_name, Type t)
    {
        int address = GetProcAddress(lib_handle, func_name);
        if (address == 0)
            return null;
        else
            return Marshal.GetDelegateForFunctionPointer(new IntPtr(address), t);
    }

#endif
}