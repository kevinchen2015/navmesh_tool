
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.AI;


public class NavEditorWindow : EditorWindow
{
	static public NavEditorWindow instance;

	[MenuItem("Window/NavEditorWindow")]
    public static EditorWindow OpenBattleDebugWindow()
    {
        EditorWindow window = GetWindow(typeof(NavEditorWindow));
	    window.name = "NavEditorWindow";
        window.titleContent = new GUIContent("NavEditorWindow");
        window.Show();

        return window;
    }

    //--------------------------------------------------------------
    
    enum EditorMode
    {
        EDITOR = 0,
        TEST,
    }

    private EditorMode mode_ = EditorMode.EDITOR;

    private string root_paht_ = "";
    private string geom_path_ = "";
    private string navmesh_path_ = "";

    void OnEnable()
    {
        instance = this;
        root_paht_ = Application.dataPath + "/";

        nav_editor.editor_set_log_callback(nav_editor.default_unity_log);
        nav_editor.editor_init();
        nav_editor.editor_set_nav_mesh_changed_cb(this.OnNavMeshChanged);

        //暂时按现有使用的类型
        nav_editor.editor_set_nav_mesh_type(nav_editor.NavmeshType.NAVMESH_TILEMESH);
       
    }
	
	void OnDisable()
    {
        nav_editor.editor_uninit();
        instance = null;
    }

    /*
	void OnDoEnevent()
    {
	}
	*/

	void Update()
	{
        
	}

    const int button_hight = 20;
    void OnGUI()
    {
        GUILayout.BeginVertical();
        {
            //tool bar
            GUILayout.BeginHorizontal();
            {
                TopToolbar();
            }
            GUILayout.EndHorizontal();

            //editor area
            if(mode_ == EditorMode.EDITOR)
            {
                EditorArea();
            }
            else if( mode_ == EditorMode.TEST)
            {

            }
        }
        GUILayout.EndVertical();
    }

    void TopToolbar()
    {
        if (GUILayout.Button("编辑模式", GUILayout.Width(130), GUILayout.Height(button_hight)))
        {
            mode_ = EditorMode.EDITOR;
        }

        if (GUILayout.Button("测试模式", GUILayout.Width(130), GUILayout.Height(button_hight)))
        {
            mode_ = EditorMode.TEST;
        }
    }

    bool CheckFileName(string file_name)
    {
        if(file_name.Length == 0)
        {
            Debug.LogError("文件名为空");
            return false;
        }

        return true;
    }

    static string MeshToString(Mesh mesh)
    {
        StringBuilder sb = new StringBuilder();

        sb.Append("g ").Append(mesh.name).Append("\n");
        foreach (Vector3 v in mesh.vertices)
        {
            sb.Append(string.Format("v {0} {1} {2}\n", v.x, v.y, v.z));
        }
        sb.Append("\n");
        foreach (Vector3 v in mesh.normals)
        {
            sb.Append(string.Format("vn {0} {1} {2}\n", v.x, v.y, v.z));
        }
        sb.Append("\n");
        foreach (Vector3 v in mesh.uv)
        {
            sb.Append(string.Format("vt {0} {1}\n", v.x, v.y));
        }
        for (int material = 0; material < mesh.subMeshCount; material++)
        {
            sb.Append("\n");

            int[] triangles = mesh.GetTriangles(material);
            for (int i = 0; i < triangles.Length; i += 3)
            {
                sb.Append(string.Format("f {0}/{0}/{0} {1}/{1}/{1} {2}/{2}/{2}\n",
                    triangles[i] + 1, triangles[i + 1] + 1, triangles[i + 2] + 1));
            }
        }
        return sb.ToString();
    }

    static void MeshToFile(Mesh mesh, string filename)
    {
        using (StreamWriter sw = new StreamWriter(filename+".obj"))
        {
            sw.Write(MeshToString(mesh));
            Debug.Log("save mesh:"+filename);
        }
    }

    void NavmeshToMeshObj(string filename)
    {
        NavMeshTriangulation triangulatedNavMesh = NavMesh.CalculateTriangulation();
        Mesh mesh = new Mesh();
        mesh.name = "NavMesh";
        mesh.vertices = triangulatedNavMesh.vertices;
        mesh.triangles = triangulatedNavMesh.indices;

        MeshToFile(mesh, filename);
    }

    void EditorArea()
    {
        GUILayout.BeginVertical();
        {
            GUILayout.BeginHorizontal();
            {
                geom_path_ = EditorGUILayout.TextField("输入模型文件(不带扩展名)", geom_path_);
                if (GUILayout.Button("加载", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(geom_path_)) return;
                    nav_editor.editor_geom_reload(root_paht_ + geom_path_);
                }
                if (GUILayout.Button("保存", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(geom_path_)) return;
                    nav_editor.editor_geom_save(root_paht_ + geom_path_);
                }
                if (GUILayout.Button("所选物件生成obj文件", GUILayout.Width(190), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(geom_path_)) return;
                    bool ret = EditorObjExporter.ExportWholeSelectionToSingleObjFile(root_paht_ + geom_path_);
                    if(ret)
                        nav_editor.editor_geom_reload(root_paht_ + geom_path_);
                }
                if (GUILayout.Button("navmesh生成obj文件", GUILayout.Width(190), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(geom_path_)) return;
                    NavmeshToMeshObj(root_paht_ + geom_path_);
                }
            }
            GUILayout.EndHorizontal();

            if (GUILayout.Button("生成导航网格", GUILayout.Width(130), GUILayout.Height(button_hight)))
            {
                int ret = nav_editor.editor_nav_mesh_build();
            }

            GUILayout.BeginHorizontal();
            {
                navmesh_path_ = EditorGUILayout.TextField("输入导航网格文件名(不带扩展名)", navmesh_path_);
                if (GUILayout.Button("加载", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(navmesh_path_)) return;
                    nav_editor.editor_load_nav_mesh(root_paht_ + navmesh_path_ + ".bin");
                }
                if (GUILayout.Button("保存", GUILayout.Width(130), GUILayout.Height(button_hight)))
                {
                    if (!CheckFileName(navmesh_path_)) return;
                    nav_editor.editor_save_nav_mesh(root_paht_ + navmesh_path_ + ".bin");
                }
            }
            GUILayout.EndHorizontal();
        }
        GUILayout.EndVertical();
    }

    void OnNavMeshChanged()
    {
        Debug.Log("navmesh changed !");
        //todo...

    }

}
