package com.kronos3d.ui.hud

import android.content.Context
import android.graphics.Color
import android.widget.TextView

class HudOverlay(context: Context) {
    val view = TextView(context).apply {
        setBackgroundColor(Color.parseColor("#99000000"))
        setTextColor(Color.WHITE)
        textSize = 15f
        setPadding(20, 15, 20, 15)
        text = "FPS: 0.0\nVerts: 0\nFaces: 0"
    }

    fun updateStats(fps: Float, verts: Int, faces: Int) {
        view.text = String.format("FPS: %.1f\nVerts: %d\nFaces: %d", fps, verts, faces)
    }
}
