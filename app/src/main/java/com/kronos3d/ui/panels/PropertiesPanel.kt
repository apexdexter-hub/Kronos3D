package com.kronos3d.ui.panels

import android.content.Context
import android.graphics.Color
import android.widget.LinearLayout
import android.widget.TextView

class PropertiesPanel(context: Context) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#88000000"))
        setPadding(15, 15, 15, 15)
        
        val title = TextView(context).apply {
            text = "Properties"
            setTextColor(Color.WHITE)
            textSize = 16f
        }
        addView(title)
    }
}
