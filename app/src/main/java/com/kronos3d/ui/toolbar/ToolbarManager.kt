package com.kronos3d.ui.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.StateListDrawable
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import com.kronos3d.ui.KronosGLSurfaceView

class ToolsToolbarManager(private val context: Context, private val glView: KronosGLSurfaceView) {
    val view = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setBackgroundColor(Color.parseColor("#CC11111e")) // Blender dark navy layout
        setPadding(20, 20, 20, 20)
    }

    private var activeTool = "SEL"
    private val buttonsList = mutableListOf<Button>()

    // Global Action Buttons Layout (Always visible and unlocked: Move, Scale/Resize, Rotate, Select)
    private val globalToolsLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
    }

    // Mesh operation specific buttons (Extrude, Subdivide, Reset) - Only relevant in Edit Mode
    private val meshOpsLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
    }

    init {
        setupGlobalToolButtons()
        setupMeshOpButtons()
        
        view.addView(globalToolsLayout)
        view.addView(meshOpsLayout)
        
        setEditMode(false) // Default: Object Mode hide ops
        selectButton("SEL")
    }

    private fun getNormalDrawable(strokeColor: String = "#4f4f7a"): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = 16f
            setColor(Color.parseColor("#E61f1f2e"))
            setStroke(3, Color.parseColor(strokeColor))
        }
    }

    private fun getActiveDrawable(strokeColor: String = "#ffaa00"): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = 16f
            setColor(Color.parseColor("#E62d2d4a"))
            setStroke(4, Color.parseColor(strokeColor))
        }
    }

    private fun makeModernButton(textLabel: String, tag: String, strokeCol: String, activeCol: String, onClick: () -> Unit): Button {
        val btn = Button(context).apply {
            text = textLabel
            setTextColor(Color.WHITE)
            background = getNormalDrawable(strokeCol)
            textSize = 12f
            layoutParams = LinearLayout.LayoutParams(160, 110).apply {
                setMargins(0, 10, 0, 10)
            }
            setOnClickListener {
                onClick()
                selectButton(tag)
            }
        }
        btn.tag = tag
        buttonsList.add(btn)
        return btn
    }

    private fun selectButton(tag: String) {
        activeTool = tag
        for (btn in buttonsList) {
            val btnTag = btn.tag as? String
            if (btnTag == tag) {
                // Color active accent depending on tool type
                val color = when(tag) {
                    "MOV" -> "#ff3333" // Red
                    "ROT" -> "#33cc33" // Green
                    "SCL" -> "#3333ff" // Blue
                    "SEL" -> "#ffaa00" // Yellow
                    else -> "#9999ff"
                }
                btn.background = getActiveDrawable(color)
            } else {
                btn.background = getNormalDrawable()
            }
        }
    }

    fun setEditMode(isEdit: Boolean) {
        view.post {
            if (isEdit) {
                meshOpsLayout.visibility = View.VISIBLE
            } else {
                meshOpsLayout.visibility = View.GONE
            }
        }
    }

    private fun setupGlobalToolButtons() {
        // 1. SELECT Tool Button
        val selBtn = makeModernButton("SELECT", "SEL", "#8888aa", "#ffaa00") {
            glView.queueEvent {
                glView.renderer.nativeSetToolSelect()
            }
        }
        globalToolsLayout.addView(selBtn)

        // 2. MOVE Tool Button (Red theme)
        val movBtn = makeModernButton("MOVE", "MOV", "#aa5555", "#ff3333") {
            glView.queueEvent {
                glView.renderer.nativeSetToolMove()
            }
        }
        globalToolsLayout.addView(movBtn)

        // 3. SCALE Tool Button (Blue theme)
        val sclBtn = makeModernButton("RESIZE", "SCL", "#5555aa", "#3333ff") {
            glView.queueEvent {
                glView.renderer.nativeSetToolScale()
            }
        }
        globalToolsLayout.addView(sclBtn)

        // 4. ROTATE Tool Button (Green theme)
        val rotBtn = makeModernButton("ROTATE", "ROT", "#55aa55", "#33cc33") {
            glView.queueEvent {
                glView.renderer.nativeSetToolRotate()
            }
        }
        globalToolsLayout.addView(rotBtn)

        // 5. LIGHT COLOR Action Button (always visible)
        val ltColBtn = Button(context).apply {
            text = "LT COLOR"
            setTextColor(Color.WHITE)
            background = getNormalDrawable("#aa55aa")
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(160, 110).apply {
                setMargins(0, 10, 0, 10)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeCycleLightColor()
                }
            }
        }
        globalToolsLayout.addView(ltColBtn)
    }

    private fun setupMeshOpButtons() {
        // SUBDIVIDE Mesh Op
        val subBtn = Button(context).apply {
            text = "SUBDIV"
            setTextColor(Color.WHITE)
            background = getNormalDrawable("#5588aa")
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(160, 110).apply {
                setMargins(0, 10, 0, 10)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeSubdivide()
                }
            }
        }
        meshOpsLayout.addView(subBtn)

        // EXTRUDE Mesh Op
        val extBtn = Button(context).apply {
            text = "EXTRUDE"
            setTextColor(Color.WHITE)
            background = getNormalDrawable("#aa8855")
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(160, 110).apply {
                setMargins(0, 10, 0, 10)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeExtrude()
                }
            }
        }
        meshOpsLayout.addView(extBtn)

        // UNDO Mesh Op
        val undoBtn = Button(context).apply {
            text = "UNDO"
            setTextColor(Color.WHITE)
            background = getNormalDrawable("#aa5555")
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(160, 110).apply {
                setMargins(0, 10, 0, 10)
            }
            setOnClickListener {
                glView.queueEvent {
                    glView.renderer.nativeUndo()
                    selectButton("SEL")
                    glView.renderer.nativeSetToolSelect()
                }
            }
        }
        meshOpsLayout.addView(undoBtn)
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
            text = "OBJECT MODE"
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
                text = if (isEditMode) "EDIT MODE" else "OBJECT MODE"
                glView.queueEvent {
                    glView.renderer.nativeToggleEditMode()
                }
                onModeChanged(isEditMode)
            }
        }
        view.addView(modeBtn)
    }
}
