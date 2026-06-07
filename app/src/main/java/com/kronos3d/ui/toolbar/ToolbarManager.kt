package com.kronos3d.ui.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import com.kronos3d.ui.KronosGLSurfaceView

class ToolsToolbarManager(private val context: Context, private val glView: KronosGLSurfaceView) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#CC1a1a2e"))
        setPadding(15, 15, 15, 15)
    }

    private var activeTool = "SEL"

    private val objectModeLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
    }

    private val editModeLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
    }

    init {
        setupObjectModeButtons()
        setupEditModeButtons()
        
        view.addView(objectModeLayout)
        view.addView(editModeLayout)
        
        setEditMode(false)
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

    fun setEditMode(isEdit: Boolean) {
        view.post {
            if (isEdit) {
                objectModeLayout.visibility = View.GONE
                editModeLayout.visibility = View.VISIBLE
            } else {
                objectModeLayout.visibility = View.VISIBLE
                editModeLayout.visibility = View.GONE
            }
        }
    }

    private fun setupObjectModeButtons() {
        val movBtn = Button(context).apply {
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
                }
            }
        }
        objectModeLayout.addView(movBtn)

        val rotBtn = Button(context).apply {
            text = "🔄 ROT"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSetToolRotate()
                    activeTool = "ROT"
                }
            }
        }
        objectModeLayout.addView(rotBtn)

        val sclBtn = Button(context).apply {
            text = "📐 SCL"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSetToolScale()
                    activeTool = "SCL"
                }
            }
        }
        objectModeLayout.addView(sclBtn)
    }

    private fun setupEditModeButtons() {
        val selBtn = Button(context).apply {
            text = "🔲 SEL"
            setTextColor(Color.WHITE)
            background = getActiveDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSetToolSelect()
                    activeTool = "SEL"
                }
            }
        }
        editModeLayout.addView(selBtn)

        val subBtn = Button(context).apply {
            text = "➕ SUB"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSubdivide()
                }
            }
        }
        editModeLayout.addView(subBtn)

        val extBtn = Button(context).apply {
            text = "⬆️ EXT"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeExtrude()
                }
            }
        }
        editModeLayout.addView(extBtn)

        val rstBtn = Button(context).apply {
            text = "🔄 RST"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeResetMesh()
                    activeTool = "SEL"
                }
            }
        }
        editModeLayout.addView(rstBtn)
    }
}

class ModeSwitchManager(private val context: Context, private val glView: KronosGLSurfaceView, private val onModeChanged: (Boolean) -> Unit) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#CC1a1a2e"))
        setPadding(15, 15, 15, 15)
    }
    
    private var isEditMode = false

    init {
        val modeBtn = Button(context).apply {
            text = "📦 OBJ MODE"
            setTextColor(Color.WHITE)
            background = GradientDrawable().apply {
                shape = GradientDrawable.RECTANGLE
                cornerRadius = 12f
                setColor(Color.parseColor("#CC2d2d44"))
                setStroke(2, Color.parseColor("#6666aa"))
            }
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(300, 120).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                isEditMode = !isEditMode
                text = if (isEditMode) "✏️ EDIT MODE" else "📦 OBJ MODE"
                glView.queueEvent {
                    glView.renderer.nativeToggleEditMode()
                }
                onModeChanged(isEditMode)
            }
        }
        view.addView(modeBtn)
    }
}
