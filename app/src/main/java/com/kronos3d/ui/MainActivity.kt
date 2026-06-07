package com.kronos3d.ui

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.ViewGroup
import android.view.Window
import android.view.WindowManager
import android.widget.FrameLayout
import com.kronos3d.ui.hud.HudOverlay
import com.kronos3d.ui.toolbar.ToolsToolbarManager
import com.kronos3d.ui.toolbar.ModeSwitchManager

class MainActivity : Activity() {
    private var glView: KronosGLSurfaceView? = null
    private var hudOverlay: HudOverlay? = null
    private val handler = Handler(Looper.getMainLooper())
    private var updateRunnable: Runnable? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val rootLayout = FrameLayout(this).apply {
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        val surfaceView = KronosGLSurfaceView(this)
        glView = surfaceView
        rootLayout.addView(surfaceView)

        val hud = HudOverlay(this)
        hudOverlay = hud
        hud.view.layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.TOP or Gravity.LEFT
            setMargins(30, 30, 0, 0)
        }
        rootLayout.addView(hud.view)

        val toolsToolbar = ToolsToolbarManager(this, surfaceView)
        toolsToolbar.view.layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.LEFT or Gravity.CENTER_VERTICAL
            setMargins(30, 0, 0, 0)
        }
        rootLayout.addView(toolsToolbar.view)

        val modeSwitch = ModeSwitchManager(this, surfaceView) { isEditMode ->
            toolsToolbar.setEditMode(isEditMode)
        }
        modeSwitch.view.layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.RIGHT or Gravity.TOP
            setMargins(0, 30, 30, 0)
        }
        rootLayout.addView(modeSwitch.view)

        setContentView(rootLayout)

        updateRunnable = object : Runnable {
            override fun run() {
                val fps = surfaceView.renderer.nativeGetFPS()
                val verts = surfaceView.renderer.nativeGetVertCount()
                val faces = surfaceView.renderer.nativeGetFaceCount()
                
                hudOverlay?.updateStats(fps, verts, faces)
                handler.postDelayed(this, 500)
            }
        }
        handler.post(updateRunnable!!)
    }

    override fun onResume() {
        super.onResume()
        glView?.onResume()
        updateRunnable?.let { handler.post(it) }
    }

    override fun onPause() {
        super.onPause()
        glView?.queueEvent {
            glView?.renderer?.nativeSaveMesh()
        }
        glView?.onPause()
        updateRunnable?.let { handler.removeCallbacks(it) }
    }
}
