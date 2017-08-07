
using UnityEngine;


[ExecuteInEditMode]
public class NavMeshHelper : MonoBehaviour
{
    public Color[] area_color;

#if UNITY_EDITOR

    void Awake()
    {
        if(area_color == null)
        {
            area_color = new Color[ConvexUtilities.area_max];
            for(int i = 0; i < ConvexUtilities.area_max;++i)
            {
                area_color[i] = new Color(0, 0, 0, 1);
            }

            //默认色
            area_color[0] = Color.yellow;
            area_color[1] = Color.blue;
            area_color[2] = Color.white;
            area_color[3] = Color.red;
            area_color[4] = Color.green;
            area_color[5] = Color.black;


            area_color[6] = Color.black;
            area_color[6].a = 0.5f;

            area_color[7] = Color.grey;
        }
    }


    NavMeshRender navmesh_render_ = new NavMeshRender();

    void OnEnable()
    {
        navmesh_render_.parent_ = transform;
        navmesh_render_.SetColorRef(ref area_color);

        nav_editor.editor_set_log_callback(nav_editor.default_unity_log);
        nav_editor.editor_init();
        nav_editor.editor_set_nav_mesh_changed_cb(this.OnNavMeshChanged);

        //暂时按现有使用的类型
        nav_editor.editor_set_nav_mesh_type(nav_editor.NavmeshType.NAVMESH_TILEMESH);

    }

    void OnDisable()
    {
        nav_editor.editor_uninit();
    }

    void OnDrawGizmos()
    {

    }

    public void RefreshNavmeshRender(nav_editor.MyNavmeshRenderInfo info)
    {
        navmesh_render_.Refresh(info);
    }

    public void ClearRefreshNavmesh()
    {
        navmesh_render_.Clear();
    }

#region private function

    private void OnNavMeshChanged()
    {
        
    }

#endregion


#endif

}