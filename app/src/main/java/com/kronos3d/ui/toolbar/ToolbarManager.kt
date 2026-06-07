package com.kronos3d.ui.toolbar

import android.content.Context
import android.graphics.Color
import android.widget.Button
import android.widget.LinearLayout
import com.kronos3d.ui.KronosGLSurfaceView

class ToolbarManager(private val context: Context, private val glView: KronosGLSurfaceView) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#aa000000"))
        setPadding(15, 15, 15, 15)
    }

    private var editButton: Button? = null

    init {
        setupButtons()
    }

    private fun setupButtons() {
        editButton = Button(context).apply {
            text = "EDIT"
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#44FFFFFF"))
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeToggleEditMode()
                    post {
                        text = if (text == "EDIT") "OBJ" else "EDIT"
                    }
                }
            }
        }
        view.addView(editButton)

        view.addView(createButton("SEL") { glView.renderer.nativeSetToolSelect() })
        view.addView(createButton("MOV") { glView.renderer.nativeSetToolMove() })
        view.addView(createButton("EXT") { glView.renderer.nativeExtrude() })
        view.addView(createButton("SUB") { glView.renderer.nativeSubdivide() })
        view.addView(createButton("RST") {
            glView.renderer.nativeResetMesh()
            post {
                editButton?.text = "EDIT"
            }
        })
    }

    private fun createButton(label: String, action: () -> Unit): Button {
        return Button(context).apply {
            text = label
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#44FFFFFF"))
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent { action() }
            }
        }
    }
}
