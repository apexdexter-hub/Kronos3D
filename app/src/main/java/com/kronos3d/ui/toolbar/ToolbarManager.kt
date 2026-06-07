package com.kronos3d.ui.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.widget.Button
import android.widget.LinearLayout
import com.kronos3d.ui.KronosGLSurfaceView

class ToolbarManager(private val context: Context, private val glView: KronosGLSurfaceView) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#CC1a1a2e"))
        setPadding(15, 15, 15, 15)
    }

    private var editButton: Button? = null
    private var selButton: Button? = null
    private var movButton: Button? = null

    // Track active mode/tool in Kotlin for UI highlight
    private var isEditMode = false
    private var activeTool = "SEL" // "SEL" or "MOV"

    init {
        setupButtons()
    }

    private fun getNormalDrawable(): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = 12f
            setColor(Color.parseColor("#CC2d2d44"))
            setStroke(2, Color.parseColor("#6666aa"))
        }
    }

    private fun getActiveDrawable(): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = 12f
            setColor(Color.parseColor("#CC4444ff"))
            setStroke(2, Color.parseColor("#9999ff"))
        }
    }

    private fun updateButtonStyles() {
        view.post {
            editButton?.background = if (isEditMode) getActiveDrawable() else getNormalDrawable()
            editButton?.text = if (isEditMode) "✏️ OBJ" else "✏️ EDIT"

            selButton?.background = if (activeTool == "SEL") getActiveDrawable() else getNormalDrawable()
            movButton?.background = if (activeTool == "MOV") getActiveDrawable() else getNormalDrawable()
        }
    }

    private fun setupButtons() {
        editButton = Button(context).apply {
            text = "✏️ EDIT"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeToggleEditMode()
                    isEditMode = !isEditMode
                    updateButtonStyles()
                }
            }
        }
        view.addView(editButton)

        selButton = Button(context).apply {
            text = "🔲 SEL"
            setTextColor(Color.WHITE)
            background = getActiveDrawable() // SEL starts active
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSetToolSelect()
                    activeTool = "SEL"
                    updateButtonStyles()
                }
            }
        }
        view.addView(selButton)

        movButton = Button(context).apply {
            text = "↔️ MOV"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSetToolMove()
                    activeTool = "MOV"
                    updateButtonStyles()
                }
            }
        }
        view.addView(movButton)

        view.addView(createActionButton("⬆️ EXT") { glView.renderer.nativeExtrude() })
        view.addView(createActionButton("➕ SUB") { glView.renderer.nativeSubdivide() })
        view.addView(createActionButton("🔄 RST") {
            glView.renderer.nativeResetMesh()
            isEditMode = false
            activeTool = "SEL"
            updateButtonStyles()
        })
    }

    private fun createActionButton(label: String, action: () -> Unit): Button {
        return Button(context).apply {
            text = label
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent { action() }
            }
        }
    }
}
