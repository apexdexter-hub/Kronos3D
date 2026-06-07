package com.kronos3d

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.ViewGroup
import android.view.Window
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    private var glView: KronosGLSurfaceView? = null
    private var hudText: TextView? = null
    private val handler = Handler(Looper.getMainLooper())
    private var updateRunnable: Runnable? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Fullscreen setup
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Root FrameLayout holding Viewport + HUD + Toolbar overlay
        val rootLayout = FrameLayout(this).apply {
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        // 1. GL Viewport
        val surfaceView = KronosGLSurfaceView(this)
        glView = surfaceView
        rootLayout.addView(surfaceView)

        // 2. HUD text (top left)
        val hud = TextView(this).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                gravity = Gravity.TOP or Gravity.LEFT
                setMargins(30, 30, 0, 0)
            }
            setBackgroundColor(Color.parseColor("#99000000")) // Semi-transparent black
            setTextColor(Color.WHITE)
            textSize = 15f
            setPadding(20, 15, 20, 15)
            text = "FPS: 0.0\nVerts: 0\nFaces: 0"
        }
        hudText = hud
        rootLayout.addView(hud)

        // 3. Left vertical toolbar (LinearLayout)
        val toolbar = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                gravity = Gravity.LEFT or Gravity.CENTER_VERTICAL
                setMargins(30, 0, 0, 0)
            }
            setBackgroundColor(Color.parseColor("#aa000000")) // Semitransparente negro
            setPadding(15, 15, 15, 15)
        }

        // Helper to construct toolbar buttons matching the design spec
        fun createToolbarButton(label: String, onClick: () -> Unit): Button {
            return Button(this).apply {
                text = label
                setTextColor(Color.WHITE)
                setBackgroundColor(Color.parseColor("#44FFFFFF")) // Semi-transparent white
                textSize = 14f
                layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                    setMargins(0, 8, 0, 8)
                }
                setOnClickListener {
                    surfaceView.queueEvent {
                        onClick()
                    }
                }
            }
        }

        // EDIT button toggling mode
        val editButton = Button(this).apply {
            text = "EDIT"
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#44FFFFFF"))
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                surfaceView.queueEvent {
                    surfaceView.renderer.nativeToggleEditMode()
                    runOnUiThread {
                        text = if (text == "EDIT") "OBJ" else "EDIT"
                    }
                }
            }
        }
        toolbar.addView(editButton)

        // Add other vertical tools
        toolbar.addView(createToolbarButton("SEL") { surfaceView.renderer.nativeSetToolSelect() })
        toolbar.addView(createToolbarButton("MOV") { surfaceView.renderer.nativeSetToolMove() })
        toolbar.addView(createToolbarButton("EXT") { surfaceView.renderer.nativeExtrude() })
        toolbar.addView(createToolbarButton("SUB") { surfaceView.renderer.nativeSubdivide() })
        toolbar.addView(createToolbarButton("RST") {
            surfaceView.renderer.nativeResetMesh()
            runOnUiThread {
                editButton.text = "EDIT"
            }
        })

        rootLayout.addView(toolbar)
        setContentView(rootLayout)

        // Periodical HUD stats update loop (500ms)
        updateRunnable = object : Runnable {
            override fun run() {
                val fps = surfaceView.renderer.nativeGetFPS()
                val verts = surfaceView.renderer.nativeGetVertCount()
                val faces = surfaceView.renderer.nativeGetFaceCount()
                
                hudText?.text = String.format("FPS: %.1f\nVerts: %d\nFaces: %d", fps, verts, faces)
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
        glView?.onPause()
        updateRunnable?.let { handler.removeCallbacks(it) }
    }
}
