using System;
using UnityEngine;
using System.Collections.Generic;
using System.Runtime.InteropServices;


//navmesh绘制
public class NavMeshRender
{
    public Transform parent_;

#if UNITY_EDITOR

    Color[] color_;
    public void SetColorRef(ref Color[] color)
    {
        color_ = color;
    }

    public void Refresh(nav_editor.MyNavmeshRenderInfo info)
    {
        Clear();

        for (int i = 0;i < info.poly_num;++i)
        {
            IntPtr p = new IntPtr(info.poly.ToInt64() + (uint)(i * Marshal.SizeOf(typeof(nav_editor.MyPoly))));
            nav_editor.MyPoly ploy = nav_editor.PtrToStructure<nav_editor.MyPoly>(p);

            CreatePloyObj(ploy,i);
        }
    }

    public void Clear()
    {
        List<Transform> child_trans = new List<Transform>();
        int child_num = parent_.childCount;
        for (int i = 0; i < child_num; ++i)
        {
            child_trans.Add(parent_.GetChild(i));
        }

        for (int i = 0; i < child_trans.Count; ++i)
        {
            Transform child = child_trans[i];
            child.parent = null;
            GameObject.DestroyImmediate(child.gameObject);
        }
    }

 #region private function

    void CreatePloyObj(nav_editor.MyPoly poly,int id)
    {
        GameObject obj = new GameObject();
        obj.name = "PolyMesh_" + id.ToString();
        obj.transform.parent = parent_;
        obj.transform.localPosition = Vector3.zero;
        obj.transform.localScale = Vector3.one;
        obj.transform.localRotation = Quaternion.Euler(Vector3.zero);

        MeshFilter mesh_filter = obj.AddComponent<MeshFilter>();
        MeshRenderer mesh_render = obj.AddComponent<MeshRenderer>();
        mesh_render.material = new Material(Shader.Find("unlit/vertex_color"));

        Mesh mesh = new Mesh();
        mesh.name = obj.name;

        mesh_filter.sharedMesh = mesh;

        //转换数据
        Vector3[] vertices = new Vector3[poly.vertex_num];
        int[] indexes = new int[poly.index_num];
        Color[] colors = new Color[poly.vertex_num];

        for(int i = 0;i< poly.vertex_num;++i)
        {
            IntPtr p = new IntPtr(poly.vertexs.ToInt64() + (uint)(i * Marshal.SizeOf(typeof(nav_editor.MyVertex))));
            nav_editor.MyVertex v = nav_editor.PtrToStructure<nav_editor.MyVertex>(p);
            vertices[i].x = -v.v[0]; //注意!
            vertices[i].y = v.v[1];
            vertices[i].z = v.v[2];

            colors[i] = GetColor(poly.area);
            //colors[i].a = 0.3f;
        }

        for (int i = 0; i < poly.index_num; ++i)
        {
            IntPtr p = new IntPtr(poly.indexs.ToInt64() + (uint)(i * Marshal.SizeOf(typeof(int))));
            indexes[i] = nav_editor.PtrToStructure<int>(p);
        }

        Transform trans = obj.transform;
        for (int i = 0; i < vertices.Length; ++i)
        {
            vertices[i] = trans.InverseTransformPoint(vertices[i]);
        }
        mesh.vertices = vertices;
        mesh.triangles = indexes;
        mesh.colors = colors;
    }

    Color GetColor(byte area)
    {
        Color color = Color.yellow;
        color = color_[(int)area];
        return color;
    }

    #endregion

#endif

}

//------------------------------------------------------------------------------------


