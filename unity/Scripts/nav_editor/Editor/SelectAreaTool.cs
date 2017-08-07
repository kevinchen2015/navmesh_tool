
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;


//选择，删除区域工具
//点击区域内部，显示区域属性
//按键 “ ” 删除所选区域

public class SelectAreaTool : ToolBase
{
    int current_area_;

    ConvexUtilities convex_ = new ConvexUtilities();
    int selected_convex_id_ = -1;

    public override void OnEnter()
    {
        convex_.Init();
        convex_.ReloadConvexInfo();
    }

    public override void OnExit()
    {
        convex_.Uninit();
    }

    public override void DoEvent()
    {
        if (Event.current.type == EventType.MouseDown)
        {
            if (Event.current.button == 0)
            {
               //nothing
            }
            else
            {
                return;
            }

            Vector3 hitPos = Vector3.zero;
            Ray ray = HandleUtility.GUIPointToWorldRay(Event.current.mousePosition);
            RaycastHit[] hits = Physics.RaycastAll(ray);
            if (hits.Length > 0)
            {
                float dist = Mathf.Infinity;
                for (int i = 0; i < hits.Length; i++)
                {
                    if (dist < hits[i].distance)
                    {
                        continue;
                    }

                    Vector3 pos = hits[i].point;
                    Event.current.Use();

                    selected_convex_id_ = convex_.QueryConvex(pos);
                    OnSelected();
                
                    break;
                }
            }
        }
        else if(Event.current.type == EventType.KeyDown)
        {
           if(Event.current.keyCode == KeyCode.Delete)
            {
                Event.current.Use();
                if (selected_convex_id_ >= 0)
                {
                    convex_.RemoveConvex(selected_convex_id_);
                    selected_convex_id_ = -1;
                    OnSelected();
                }
            }
        }
    }

    public override void OnGUI()
    {
        if(selected_convex_id_ >= 0)
        {
            current_area_ = EditorGUILayout.IntSlider("area id", current_area_, 1, ConvexUtilities.area_max);

            if (GUILayout.Button("生效修改", GUILayout.Width(130), GUILayout.Height(20)))
            {
                convex_.ModifyConvex(selected_convex_id_, current_area_);
                NavMeshHelperEditor.instance.Build();
                convex_.ReloadConvexInfo();
                selected_convex_id_ = -1;
            }
        }
    }

    public override void DoPaint()
    {
        convex_.DoPaintConvex();
    }

 #region private function

    void OnSelected()
    {
        if (selected_convex_id_ < 0)
        {
            current_area_ = 0;
        }
        else
        {
            ConvexInfo convex = convex_.GetConvexById(selected_convex_id_);
            current_area_ = (int)convex.area;
        }
    }
    
#endregion

}



