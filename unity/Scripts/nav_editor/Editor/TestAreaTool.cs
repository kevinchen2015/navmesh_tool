using System;
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;
using System.Runtime.InteropServices;


//测试运行时区域flag改变


public class TestAreaTool : ToolBase
{
    uint select_poly_ref_ = 0;
    ushort select_poly_flag_ = 0;
    byte select_poly_area_ = 0;

    nav_editor.MyVertex p = new nav_editor.MyVertex();
    bool[] flag_toggle_ = new bool[ConvexUtilities.flag_edit_num];

    public override void OnEnter()
    {

    }

    public override void OnExit()
    {

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

                    QueryAreaFlag(pos);

                    break;
                }
            }
        }
       
    }

    public override void OnGUI()
    {
        if (select_poly_ref_ > 0)
        {
            EditorGUILayout.LabelField("多边区域id:" + select_poly_ref_.ToString());

            GUILayout.Label("多边形标记位");
            for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
            {
                flag_toggle_[i] = EditorGUILayout.Toggle(nav_editor.FlagName[i], flag_toggle_[i]);
            }
            GUILayout.Space(5);

            select_poly_area_ = (byte)EditorGUILayout.IntSlider("多边区域值", (int)select_poly_area_, 0, ConvexUtilities.area_max);

            if (GUILayout.Button("生效修改", GUILayout.Width(130), GUILayout.Height(20)))
            {

                for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
                {
                    if (flag_toggle_[i])
                    {
                        select_poly_flag_ |= (ushort)(1 << i);
                    }
                    else
                    {
                        select_poly_flag_ &= (ushort)(~(1 << i));
                    }
                }

                nav_editor.editor_nav_mesh_modify(select_poly_ref_, select_poly_flag_, select_poly_area_);
                
                //select_poly_ref_ = 0;
                Refresh();

                NavMeshHelperEditor.instance.MakeNavMesh();
            }
        }
    }

    public override void DoPaint()
    {
       
    }

#region

    void QueryAreaFlag(Vector3 pos)
    {
        p.v = new float[3];
        p.v[0] = - pos.x;
        p.v[1] = pos.y;
        p.v[2] = pos.z;

        Refresh();
    }

    void Refresh()
    {
        nav_editor.editor_nav_mesh_query(ref p, ref select_poly_ref_, ref select_poly_flag_, ref select_poly_area_);

        for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
        {
            ushort n = ((ushort)(1 << i));
            n &= select_poly_flag_;
            flag_toggle_[i] = n > 0 ? true : false;
        }
    }

#endregion

}



