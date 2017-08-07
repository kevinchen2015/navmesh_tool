using System;
using UnityEditor;
using UnityEngine;
using System.Runtime.InteropServices;
using System.IO;
using System.Text;



public class ToolBase
{
    public virtual void OnEnter() { }
    public virtual void OnExit()  { }
    public virtual void DoEvent() { }
    public virtual void DoPaint() { }
    public virtual void OnGUI()   { }
}


[CustomEditor(typeof(NavMeshHelper))]
public class NavMeshHelperEditor : Editor
{
    public static NavMeshHelperEditor instance;

    const int button_hight = 20;
    public enum ToolType
    {
        None = 0,
        TestNavMesh,
        SelectArea,
        AddArea,
        EditAreaColor,
        TestArea,
        Max
    }

    ToolType tool_type_ = ToolType.None;
    ToolBase[] tools_ = new ToolBase[(int)ToolType.Max];

    string root_paht_;
    string file_name_;
    public  NavMeshHelper helper_;
    nav_editor.MyNavmeshRenderInfo navmesh_render_info_;

    private void OnEnable()
    {
        instance = this;
        helper_ = (NavMeshHelper)target;

        root_paht_ = Application.dataPath + "/";

        tools_[(int)ToolType.None] = new ToolBase();
        tools_[(int)ToolType.TestNavMesh] = new TestNavMeshTool();
        tools_[(int)ToolType.SelectArea] = new SelectAreaTool();
        tools_[(int)ToolType.AddArea] = new AddAreaTool();
        tools_[(int)ToolType.EditAreaColor] = new EditAreaColorTool();
        tools_[(int)ToolType.TestArea] = new TestAreaTool();

        DataInit();

        tools_[(int)tool_type_].OnEnter();
    }

    private void OnDisable()
    {
        tools_[(int)tool_type_].OnExit();

        helper_.ClearRefreshNavmesh();

        instance = null;
        helper_ = null;

        DataUninit();
    }

    public override void OnInspectorGUI() 
	{
        GUILayout.BeginVertical();
        {
        //--------------------------base-----------------------------------------------------
            GUILayout.BeginVertical();
            {
                EditorGUILayout.Space();
                file_name_ = EditorGUILayout.TextField("输入文件(不带扩展名)", file_name_);
                GUILayout.BeginHorizontal();
                {
                    if (GUILayout.Button("加载", GUILayout.Width(130), GUILayout.Height(button_hight)))
                    {
                        if (!CheckFileName(file_name_)) return;
                        LoadFile(root_paht_ + file_name_);
                    }
                    if (GUILayout.Button("所选物件生成obj文件", GUILayout.Width(190), GUILayout.Height(button_hight)))
                    {
                        if (!CheckFileName(file_name_)) return;
                        bool ret = EditorObjExporter.ExportWholeSelectionToSingleObjFile(root_paht_ + file_name_);
                        if (ret)
                        {
                            LoadFile(root_paht_ + file_name_);
                        }
                    }
                }
                GUILayout.EndHorizontal();

                EditorGUILayout.Space();
                if (GUILayout.Button("生成导航网格", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    Build();
                }
                EditorGUILayout.Space();
                if (GUILayout.Button("保存", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(file_name_)) return;
                    SaveFile(root_paht_ + file_name_);
                }

                EditorGUILayout.Space();
                if (GUILayout.Button("清除", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    helper_.ClearRefreshNavmesh();
                }
 
            }
            GUILayout.EndVertical();

        //-------------------------tools-----------------------------------------------------
            EditorGUILayout.Space();
            EditorGUILayout.Space();

            ToolType old_type = tool_type_;
            tool_type_ = (ToolType)EditorGUILayout.EnumPopup("编辑模式",tool_type_);
            if(tool_type_ == ToolType.Max)
            {
                tool_type_ = ToolType.None;
            }
            if(old_type != tool_type_)
            {
                tools_[(int)old_type].OnExit();
                tools_[(int)tool_type_].OnEnter();
            }

            EditorGUILayout.Space();
            EditorGUILayout.Space();

            tools_[(int)tool_type_].OnGUI();

        }
        GUILayout.EndVertical();
    }


    void OnSceneGUI()
    {
        if (Event.current.type == EventType.Repaint)
        {
            tools_[(int)tool_type_].DoPaint();            
        }
        else
        {
            tools_[(int)tool_type_].DoEvent();
        }
    }

    public void MakeNavMesh()
    {
        int ret = nav_editor.editor_get_nav_mesh_render_info(ref navmesh_render_info_);
        if (ret == 0)
        {
            //SaveToFile(root_paht_ + "test.txt", navmesh_render_info_);
            helper_.RefreshNavmeshRender(navmesh_render_info_);
        }
    }

    public void RefreshArea()
    {
        helper_.RefreshNavmeshRender(navmesh_render_info_);
    }

 #region  navmesh create,save,load

    void DataInit()
    {
        navmesh_render_info_ = new nav_editor.MyNavmeshRenderInfo();
        navmesh_render_info_.poly_num = 0;
        navmesh_render_info_.poly = IntPtr.Zero;

    }
    
    void DataUninit()
    {
        for(int i = 0;i < navmesh_render_info_.poly_num; ++i)
        {
            IntPtr p = new IntPtr(navmesh_render_info_.poly.ToInt64() + (uint)(i * Marshal.SizeOf(typeof(nav_editor.MyPoly))));
            nav_editor.MyPoly ploy = nav_editor.PtrToStructure<nav_editor.MyPoly>(p);

            if(ploy.vertexs != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(ploy.vertexs);
            }
            if(ploy.indexs != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(ploy.indexs);
            }
        }
        if (navmesh_render_info_.poly != IntPtr.Zero)
        {
            Marshal.FreeHGlobal(navmesh_render_info_.poly);
        }
        navmesh_render_info_.poly_num = 0;
    }


    bool CheckFileName(string file_name)
    {
        if (file_name == null || file_name.Length  == 0)
        {
            Debug.LogError("文件名为空啊!");
            return false;
        }
        return true;
    }

    void LoadFile(string file)
    {
        nav_editor.editor_geom_reload(file);
        int ret = nav_editor.editor_load_nav_mesh(file + ".bin");
        if(ret == 0)
        {
            MakeNavMesh();
        }
    }

    public void Build()
    {
        int ret = nav_editor.editor_nav_mesh_build();
        if (ret == 0)
        {
            MakeNavMesh();
        }
    }

    void SaveFile(string file)
    {
        nav_editor.editor_geom_save(file);
        nav_editor.editor_save_nav_mesh(file + ".bin");
    }




 #endregion

}
