using System;
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;
using System.Runtime.InteropServices;


//测试导航网格寻路
//shift+左键 点击行走面为起点，左键行走面为终点

public class TestNavMeshTool : ToolBase
{
    Vector3 start_pos;
    Vector3 end_pos;
    List<Vector3> pos_list = new List<Vector3>();

    ushort include_flag = 0xffff;
    ushort exclude_flag = 0x00;

    bool[] include_toggle = new bool[ConvexUtilities.flag_edit_num];
    bool[] exclude_toggle = new bool[ConvexUtilities.flag_edit_num];

    public override void OnEnter()
    {
        for(int i=0;i< ConvexUtilities.flag_edit_num; ++i)
        {
            ushort n = ((ushort)(1 << i));
            n &= include_flag;
            include_toggle[i] = n > 0 ? true : false;
        }
        for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
        {
            ushort n = ((ushort)(1 << i));
            n &= exclude_flag;
            exclude_toggle[i] = n > 0 ? true : false;
        }
    }

    public override void OnGUI()
    {
        GUILayout.BeginVertical();

        GUILayout.Label("包含标记位");
        for (int i = 0;i < ConvexUtilities.flag_edit_num;++i)
        {
            include_toggle[i] = EditorGUILayout.Toggle(nav_editor.FlagName[i], include_toggle[i]);
        }
        GUILayout.Space(5);

        GUILayout.Label("排除标记位");
        for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
        {
            exclude_toggle[i] = EditorGUILayout.Toggle(nav_editor.FlagName[i], exclude_toggle[i]);
        }

        GUILayout.EndVertical();
    }

    public override void DoEvent()
    {
        if (Event.current.type == EventType.MouseDown)
        {
            bool start_point = false;
            bool end_point = false;

            if (Event.current.button == 0 && Event.current.shift)
            {
                start_point = true;
            }
            else if (Event.current.button == 0)
            {
                end_point = true;
            }

            if(!start_point && !end_point)
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

                    if (start_point)
                        start_pos = hits[i].point;
                    if (end_point)
                        end_pos = hits[i].point;

                    Event.current.Use();
                    TestFindPath();

                    break;
                }
            }
        }
    }

    public override void DoPaint()
    {
        float size = 1.0f;

        Handles.color = Handles.xAxisColor;
        Handles.CylinderHandleCap(
        0,
        start_pos,
        Quaternion.LookRotation(Vector3.up),
        size,
        EventType.Repaint
        );

        Handles.color = Handles.zAxisColor;
        Handles.CylinderHandleCap(
        0,
        end_pos,
        Quaternion.LookRotation(Vector3.up),
        size,
        EventType.Repaint
        );

        Handles.color = Color.red;
        if (pos_list.Count > 0)
        {
            //Handles.DrawLine(start_pos, pos_list[0]);
            for (int i = 0; i < pos_list.Count - 1; ++i)
            {
                Handles.DrawLine(pos_list[i], pos_list[i + 1]);
            }
            //Handles.DrawLine(pos_list[pos_list.Count - 1], end_pos);
            
            //线段
            //Handles.DrawLines(pos_list.ToArray());
        }
    }

    #region private function

    void TestFindPath()
    {
        nav_editor.MyVertex start = new nav_editor.MyVertex();
        start.v = new float[3];
        start.v[0] = -start_pos.x;
        start.v[1] = start_pos.y;
        start.v[2] = start_pos.z;

        nav_editor.MyVertex end = new nav_editor.MyVertex();
        end.v = new float[3];
        end.v[0] = -end_pos.x;
        end.v[1] = end_pos.y;
        end.v[2] = end_pos.z;

        int num = 0;
        IntPtr vf = IntPtr.Zero;

        for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
        {
            if(include_toggle[i])
            {
                include_flag |= (ushort)(1 << i);
            }
            else
            {
                include_flag &= (ushort)(~(1 << i));
            }
        }
        for (int i = 0; i < ConvexUtilities.flag_edit_num; ++i)
        {
            if (exclude_toggle[i])
            {
                exclude_flag |= (ushort)(1 << i);
            }
            else
            {
                exclude_flag &= (ushort)(~(1 << i));
            }
        }

        pos_list.Clear();
        nav_editor.editor_nav_mesh_path_query(ref start,ref end, include_flag,exclude_flag,ref num, ref vf);

        //Debug.Log("find path,from:" + start_pos.ToString() + ",to:" + end_pos.ToString() + " num:" + num.ToString());

        for (int i = 0; i < num; ++i)
        {
            IntPtr p = new IntPtr(vf.ToInt64() + (uint)(3 * i * Marshal.SizeOf(typeof(float))));
            IntPtr p1 = new IntPtr(p.ToInt64() + 1 * Marshal.SizeOf(typeof(float)));
            IntPtr p2 = new IntPtr(p.ToInt64() + 2 * Marshal.SizeOf(typeof(float)));

            float x = -nav_editor.PtrToStructure<float>(p);
            float y = nav_editor.PtrToStructure<float>(p1);
            float z = nav_editor.PtrToStructure<float>(p2);
            Vector3 pos = new Vector3(x, y, z);

            //Debug.Log(pos.ToString());

            pos_list.Add(pos);
        }
    }

    #endregion
}



