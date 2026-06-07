package com.kronos3d.ui.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import com.kronos3d.ui.KronosGLSurfaceView

class ToolbarManager(private val context: Context, private val glView: KronosGLSurfaceView) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#CC1a1a2e"))
        setPadding(15, 15, 15, 15)
    }

    private var isEditMode = false
    private var activeTool = "SEL" // "SEL" or "MOV"

    // Layout panels for each mode
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
        
        updateVisibility()
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

    private fun updateVisibility() {
        view.post {
            if (isEditMode) {
                objectModeLayout.visibility = View.GONE
                editModeLayout.visibility = View.VISIBLE
            } else {
                objectModeLayout.visibility = View.VISIBLE
                editModeLayout.visibility = View.GONE
            }
        }
    }

    private fun setupObjectModeButtons() {
        // [EDIT] -> enter edit mode
        val editBtn = Button(context).apply {
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
                    isEditMode = true
                    updateVisibility()
                }
            }
        }
        objectModeLayout.addView(editBtn)

        // [MOV] -> move whole object (drag view orbit is disabled or maps to object shift)
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

        // [ROT] -> rotate whole object
        val rotBtn = Button(context).apply {
            text = "🔄 ROT"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            // Logic for rot can be added later or kept as selector
        }
        objectModeLayout.addView(rotBtn)

        // [SCL] -> scale whole object
        val sclBtn = Button(context).apply {
            text = "📐 SCL"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
        }
        objectModeLayout.addView(sclBtn)
    }

    private fun setupEditModeButtons() {
        // [OBJ] -> enter object mode
        val objBtn = Button(context).apply {
            text = "📦 OBJ"
            setTextColor(Color.WHITE)
            background = getNormalDrawable()
            textSize = 13f
            layoutParams = LinearLayout.LayoutParams(140, 100).apply {
                setMargins(0, 8, 0, 8)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeToggleEditMode()
                    isEditMode = false
                    updateVisibility()
                }
            }
        }
        editModeLayout.addView(objBtn)

        // [SEL] -> select tool
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

        // [SUB] -> subdivide ALL faces
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

        // [EXT] -> extrude selected face
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

        // [RST] -> reset original mesh
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
                    isEditMode = false
                    activeTool = "SEL"
                    updateVisibility()
                }
            }
        }
        editModeLayout.addView(rstBtn)
    }
}
