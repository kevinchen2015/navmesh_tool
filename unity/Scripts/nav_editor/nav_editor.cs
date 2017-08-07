using System;
using System.Runtime.InteropServices;




public delegate void event_callback();
public delegate void log_callback(int type,string msg);


public class nav_editor
{

    public enum NavmeshType
    {
        NAVMESH_SOLOMESH = 0,
        NAVMESH_TILEMESH,
        NAVMESH_TILECACHE
    }

    public enum EditorError
    {
        EDITOR_ERROR_NO_ERROR = 0,
        EDITOR_ERROR_UNKNOW,
        EDITOR_MESH_TYPE_ERROR,
        EDITOR_NAVMESH_ERROR,
    }

    //区域值
    public enum PolyAreasType
    {
        POLYAREA_GROUND = 0,
        POLYAREA_WATER,
        POLYAREA_ROAD,
        POLYAREA_DOOR,
        POLYAREA_GRASS,
        POLYAREA_JUMP,

        //新增
        POLYAREA_OBSTACLE_0,
        POLYAREA_OBSTACLE_1,
    }

    public static string[] AreaName =
    {
        "地面区域",
        "水面区域",
        "道路区域",
        "门区域",
        "草区域",
        "跳区域",

        "障碍0区域",
        "障碍1区域",
    };

    //区域标记位
    public enum PolyFlags
    {
        POLYFLAGS_WALK = 0x01,       // Ability to walk (ground, grass, road)
        POLYFLAGS_SWIM = 0x02,       // Ability to swim (water).
        POLYFLAGS_DOOR = 0x04,       // Ability to move through doors.
        POLYFLAGS_JUMP = 0x08,       // Ability to jump.
        POLYFLAGS_DISABLED = 0x10,   // Disabled polygon

        POLYFLAGS_OBSTACLE_0 = 0x20,  //新增
        POLYFLAGS_OBSTACLE_1 = 0x40,

        POLYFLAGS_ALL = 0xffff   // All abilities.
    }

    public static string[] FlagName =
    {
        "可走标记",
        "水标记",
        "门标记",
        "跳标记",
        "不可用标记",

        "障碍0标记",
        "障碍1标记",
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
    public struct MyVertex
    {
        [MarshalAsAttribute(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] v;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
    public struct MyPoly    //对应dtPoly ，取部分数据
    {
        public byte     area;
        public int      vertex_num;
        public int      index_num;
        public IntPtr   vertexs;
        public IntPtr   indexs;
        public IntPtr   org_poly;  //原始的dtPoly 地址
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
    public struct MyNavmeshRenderInfo
    {
        public int    poly_num;
        public IntPtr poly;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
    public struct MyConvexVolume
    {
        public byte area;
        public int nverts;
        public IntPtr verts;

        public float hmin;
        public float hmax;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
    public struct MyConvexVolumeInfo
    {
        public int convex_num;
        public IntPtr convex;
    }

#if UNITY_EDITOR

    const string dll_name = "nav_editor";

    [DllImport(dll_name)]
    public static extern int editor_init();
    
    [DllImport(dll_name)]
    public static extern void editor_uninit();

    [DllImport(dll_name)]
    public static extern void editor_set_log_callback(log_callback cb);

    [DllImport(dll_name)]
    public static extern int editor_geom_reload(string path);

    [DllImport(dll_name)]
    public static extern int editor_geom_save(string path);

    [DllImport(dll_name)]
    public static extern void editor_set_nav_mesh_changed_cb(event_callback cb);

    [DllImport(dll_name)]
    public static extern  void editor_set_nav_mesh_type(NavmeshType type);

    [DllImport(dll_name)]
    public static extern  int editor_nav_mesh_build();

    [DllImport(dll_name)]
    public static extern  int editor_save_nav_mesh(string path);

    [DllImport(dll_name)]
    public static extern int editor_load_nav_mesh(string path);

    [DllImport(dll_name)]
    public static extern int editor_get_nav_mesh_render_info(ref MyNavmeshRenderInfo info);

    [DllImport(dll_name)]
    public static extern int editor_nav_mesh_path_query(ref MyVertex start,ref MyVertex end,ushort include_flag,ushort exclude_flag,ref int num,ref IntPtr vf);

    [DllImport(dll_name)]
    public static extern int editor_geom_add_convex_volume(IntPtr vertext, int count, float minh, float maxh, byte area);

    [DllImport(dll_name)]
    public static extern void editor_geom_delete_convex_volume(int id);

    [DllImport(dll_name)]
    public static extern void editor_geom_get_convex_info(ref MyConvexVolumeInfo info);

    [DllImport(dll_name)]
    public static extern int editor_nav_mesh_query(ref MyVertex pos,ref uint poly_ref,ref ushort flag,ref byte area);

    [DllImport(dll_name)]
    public static extern void editor_nav_mesh_modify(uint poly_ref, ushort flag, byte area);

    public static void default_unity_log(int type,string msg)
    {
        switch(type)
        {
            case 1:
                UnityEngine.Debug.Log(msg);
                break;

            case 2:
                UnityEngine.Debug.LogWarning(msg);
                break;

            case 3:
                UnityEngine.Debug.LogError(msg);
                break;
        }
    }

    public static T PtrToStructure<T>(IntPtr ptr)
    {
        return (T)Marshal.PtrToStructure(ptr, typeof(T));
    }

    public static void StructureToPtr<T>(T structure, IntPtr ptr, bool delele_old)
    {
        Marshal.StructureToPtr(structure, ptr, delele_old);
    }

#endif

}
