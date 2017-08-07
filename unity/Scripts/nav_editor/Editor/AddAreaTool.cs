
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;


//新增区域工具
//左键点击行走面，增加一个点，shift+左键点击行走面删除最后一个点
//在最后一个点很近的地方点击，提交整个区域，最后一个点会自动补全连接到第一个点
//特别注意，点的数目有上限，编辑区域必须是凸多边形

public class AddAreaTool : ToolBase
{
    float box_escent_ = 1;
    float box_height_ = 6;


    int current_area = 0;
    List<Vector3>  current_pos_list_ = new List<Vector3>();

    ConvexUtilities convex_ = new ConvexUtilities();

    public override void OnEnter()
    {
        convex_.Init();
        convex_.ReloadConvexInfo();
    }

    public override void OnExit()
    {
        ClearCurrent();
        convex_.Uninit();
    }

    public override void DoEvent()
    {
        if (Event.current.type == EventType.MouseDown)
        {
            if (Event.current.button == 0 && Event.current.shift)
            {
                RemovePoint();
                return;
            }
            else if (Event.current.button == 0)
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
                
                    AddPoint(pos);

                    break;
                }
            }
        }
    }

    public override void OnGUI()
    {
        GUILayout.BeginVertical();

        current_area = EditorGUILayout.IntSlider("区域值", current_area, 0, ConvexUtilities.area_max);
        
        GUILayout.EndVertical();
    }

    public override void DoPaint()
    {
        //draw current
        Handles.color = Color.red;
        for (int i = 0; i < current_pos_list_.Count - 1; ++i)
        {
            Handles.CircleCap(0, current_pos_list_[i], Quaternion.LookRotation(Vector3.up), 0.5f);
        }

        for (int i = 0; i < current_pos_list_.Count - 1; ++i)
        {
            Handles.DrawLine(current_pos_list_[i], current_pos_list_[i + 1]);
        }
        //draw info
        convex_.DoPaintConvex();
    }

 #region private function

    void ClearCurrent()
    {
        current_pos_list_.Clear();

        current_area = 0;
    }

    void AddPoint(Vector3 pos)
    {
        bool submit = false;
        int count = current_pos_list_.Count;
        if (count >= ConvexUtilities.pos_num_max - 1)
        {
            submit = true;
        }
        else if(count >= 3)
        {
            Vector3 last_pos = current_pos_list_[count - 1];
            if(Vector3.Distance(last_pos, pos) < 0.2f)
            {
                submit = true;
            }
        }

        if(submit)
        {
            //最后一个点设置为第一个点
            current_pos_list_.Add(current_pos_list_[0]);
            float min = 999999999.0f;
            for(int i = 0;i < current_pos_list_.Count;++i)
            {
                if(min > current_pos_list_[i].y)
                {
                    min = current_pos_list_[i].y;
                }
            }

            float hmin = min - box_escent_;
            float hmax = hmin + box_height_;

            if (convex_.AddConvex(current_pos_list_, current_area, hmin, hmax))
            {
                ClearCurrent();
            }
        }
        else
        {
            current_pos_list_.Add(pos);
        }
    }

    void RemovePoint()
    {
        int count = current_pos_list_.Count;
        if (count == 0) return;

        current_pos_list_.RemoveAt(count - 1);
    }

#endregion

}



