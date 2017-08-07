using System;
using UnityEngine;
using System.Collections.Generic;
using System.Runtime.InteropServices;


//区域帮助类
public class ConvexUtilities
{
    public const int pos_num_max = 12;
    public const int convex_num_max = 256;
    public const int area_max = 62;   
    public const int flag_max = 16;
    public const int flag_edit_num = 7;

#if UNITY_EDITOR

    List<ConvexInfo> geom_convex_list_ = new List<ConvexInfo>();
    nav_editor.MyConvexVolumeInfo my_convex_info = new nav_editor.MyConvexVolumeInfo();

    public  void Init()
    {
        my_convex_info.convex_num = 0;
        my_convex_info.convex = IntPtr.Zero;

        ReloadConvexInfo();
    }

    public void Uninit()
    {
        geom_convex_list_.Clear();

        for (int i=0;i < my_convex_info.convex_num;++i)
        {
            IntPtr p = new IntPtr(my_convex_info.convex.ToInt64() + (uint)(i*Marshal.SizeOf(typeof(nav_editor.MyConvexVolume))));
            nav_editor.MyConvexVolume convex = nav_editor.PtrToStructure<nav_editor.MyConvexVolume>(p);
            if(convex.verts != IntPtr.Zero)
            Marshal.FreeHGlobal(convex.verts);
        }
        if (my_convex_info.convex != IntPtr.Zero)
        {
            Marshal.FreeHGlobal(my_convex_info.convex);
        }
        my_convex_info.convex_num = 0;
        my_convex_info.convex = IntPtr.Zero;
    }

    public void DoPaintConvex()
    {
        UnityEditor.Handles.color = Color.white;
        for (int i = 0; i < geom_convex_list_.Count; ++i)
        {
            ConvexInfo info = geom_convex_list_[i];
            for (int k = 0; k < info.vertexs.Count - 1; ++k)
            {
                UnityEditor.Handles.DrawLine(info.vertexs[k], info.vertexs[k + 1]);
            }
        }
    }

    public void ReloadConvexInfo()
    {
        geom_convex_list_.Clear();

        nav_editor.editor_geom_get_convex_info(ref my_convex_info);
        for(int i=0;i< my_convex_info.convex_num;++i)
        {
            IntPtr p = new IntPtr(my_convex_info.convex.ToInt64() + (uint)(i * Marshal.SizeOf(typeof(nav_editor.MyConvexVolume))));
            nav_editor.MyConvexVolume convex = nav_editor.PtrToStructure<nav_editor.MyConvexVolume>(p);

            ConvexInfo info = new ConvexInfo();
            info.id = i;
            info.hmin = convex.hmin;
            info.hmax = convex.hmax;
            info.area = convex.area;

            for(int k = 0; k < convex.nverts;++k)
            {
                IntPtr pv = new IntPtr(convex.verts.ToInt64() + (uint)(3 * k * Marshal.SizeOf(typeof(float))));
                IntPtr pv1 = new IntPtr(pv.ToInt64() + 1 * Marshal.SizeOf(typeof(float)));
                IntPtr pv2 = new IntPtr(pv.ToInt64() + 2 * Marshal.SizeOf(typeof(float)));

                Vector3 pos = Vector3.zero;
                pos.x = -nav_editor.PtrToStructure<float>(pv);
                pos.y = nav_editor.PtrToStructure<float>(pv1);
                pos.z = nav_editor.PtrToStructure<float>(pv2);

                info.vertexs.Add(pos);
            }
            geom_convex_list_.Add(info);
        }
    }

    public bool AddConvex(List<Vector3> vectex,int area,float hmin,float hmax)
    {
        if (geom_convex_list_.Count >= convex_num_max)
            return false;

        int num = vectex.Count;
        if (num < 3)
            return false;

        int size = Marshal.SizeOf(typeof(float)) * 3 * num;
        IntPtr p = Marshal.AllocHGlobal(size);

        for(int i = 0;i < num;++i)
        {
            IntPtr pv = new IntPtr(p.ToInt64() + (uint)(3 * i * Marshal.SizeOf(typeof(float))));
            IntPtr pv1 = new IntPtr(pv.ToInt64() + 1 * Marshal.SizeOf(typeof(float)));
            IntPtr pv2 = new IntPtr(pv.ToInt64() + 2 * Marshal.SizeOf(typeof(float)));

            nav_editor.StructureToPtr<float>(-vectex[i].x, pv, false);
            nav_editor.StructureToPtr<float>(vectex[i].y, pv1, false);
            nav_editor.StructureToPtr<float>(vectex[i].z, pv2, false);
        }

        int ret = nav_editor.editor_geom_add_convex_volume(p,num, hmin, hmax, (byte)area);

        Marshal.FreeHGlobal(p);

        if (ret >= 0)
        {
            ReloadConvexInfo();
        }
        else
        {
            Debug.LogError("添加区域错误");
            return false;
        }
        return true;
    }

    public void ModifyConvex(int id,int area)
    {
        ConvexInfo info = GetConvexById(id);
        if (info == null)
            return;

        nav_editor.editor_geom_delete_convex_volume(id);
        AddConvex(info.vertexs, area, info.hmin, info.hmax);
    }

    public void RemoveConvex(int id)
    {
        nav_editor.editor_geom_delete_convex_volume(id);
        ReloadConvexInfo();
    }

    public int QueryConvex(Vector3 pos)
    {
        for (int i = 0; i < geom_convex_list_.Count; ++i)
        {
            if(geom_convex_list_[i].IsPosInConvex(pos))
            {
                //Debug.Log("选中区域 id:"+ geom_convex_list_[i].id);
                return geom_convex_list_[i].id;
            }
        }
        //Debug.Log("未选中区域");
        return -1;
    }

    public ConvexInfo GetConvexById(int id)
    {
        for (int i = 0; i < geom_convex_list_.Count; ++i)
        {
            if (id == geom_convex_list_[i].id)
            {
                return geom_convex_list_[i];
            }
        }
        return null;
    }

    #region private function

    #endregion

#endif


    //static function
    public static void toggle_to_uint(bool[] toggle,ref uint n)
    {
        int result = 0;
        for(int i = 0;i <toggle.Length&&i< flag_max; ++i)
        {
            result |= (((toggle[i]) ? 1 : 0) << i);
        }
        n = (uint)result;
    }

    public static void uint_to_toggle(uint n,ref bool[] toggle)
    {
        int ni = (int)n;
        for (int i = 0; i < toggle.Length&&i< flag_max; ++i)
        {
            toggle[i] = ((ni >> i) & 0x01) > 0; 
        }
    }

}

//------------------------------------------------------------------------------------

public class ConvexInfo
{
    public byte area;
    public List<Vector3> vertexs = new List<Vector3>();
    public float hmin;
    public float hmax;
    public int id;

    public bool IsPosInConvex(Vector3 pos)
    {
        if (vertexs.Count < 3)
        {
            return false;
        }

        List<Vector3> vector = new List<Vector3>();
        for(int i = 0;i < vertexs.Count;++i)
        {
            Vector3 v = vertexs[i] - pos;
            v.Normalize();
            vector.Add(v);
        }

        Vector3 v0c1 = Vector3.Cross(vector[0], vector[1]);
        v0c1.Normalize();
        for (int i = 0; i < vector.Count-1; ++i)
        {
            Vector3 v0ci = Vector3.Cross(vector[i], vector[i+1]);
            v0ci.Normalize();

            float f = Vector3.Dot(v0c1, v0ci);
            if(f < 0)
            {
                return false;
            }
        }

        {
            Vector3 vi0c = Vector3.Cross(vector[vector.Count - 1], vector[0]);
            vi0c.Normalize();
            float f = Vector3.Dot(v0c1, vi0c);
            if (f < 0)
            {
                return false;
            }
        }
        return true;
    }
}

