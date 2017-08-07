
using UnityEditor;
using UnityEngine;
using System.Collections.Generic;


//修改区域颜色，纯粹本地显示使用

public class EditAreaColorTool : ToolBase
{
    public override void OnEnter()
    {
      
    }

    public override void OnExit()
    {
       
    }

    public override void DoEvent()
    {
        
    }

    public override void OnGUI()
    {
        GUILayout.BeginVertical();

        if (GUILayout.Button("刷新修改", GUILayout.Width(130), GUILayout.Height(20)))
        {
            NavMeshHelperEditor.instance.RefreshArea();
        }

        NavMeshHelper helper = NavMeshHelperEditor.instance.helper_;
        for (int i = 0;i < ConvexUtilities.area_max; ++i)
        {
            helper.area_color[i] = EditorGUILayout.ColorField("area id:" + i.ToString(), helper.area_color[i]);
        }

        GUILayout.EndVertical();
    }
 
}



