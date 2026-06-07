package com.kronos3d

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent

class KronosGLSurfaceView(context: Context) : GLSurfaceView(context) {
    val renderer: GLSurfaceManager

    init {
        // Require OpenGL ES 3.2
        setEGLContextClientVersion(3)
        renderer = GLSurfaceManager()
        setRenderer(renderer)
        // Render continuously to keep FPS metrics and animations fluid
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    private var prevX = 0f
    private var prevY = 0f
    private var downTime = 0L

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val x = event.x
        val y = event.y
        val action = event.actionMasked

        when (action) {
            MotionEvent.ACTION_DOWN -> {
                prevX = x
                prevY = y
                downTime = System.currentTimeMillis()
            }
            MotionEvent.ACTION_UP -> {
                val duration = System.currentTimeMillis() - downTime
                val dist = Math.hypot((x - prevX).toDouble(), (y - prevY).toDouble())
                // Tap gesture detected (less than 200ms and 15px movement delta)
                if (duration < 200 && dist < 15) {
                    val normX = (x / width) * 2.0f - 1.0f
                    val normY = -((y / height) * 2.0f - 1.0f)
                    queueEvent {
                        renderer.nativeTap(normX, normY)
                    }
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1) {
                    val dx = x - prevX
                    val dy = y - prevY
                    // Orbit controls on move
                    queueEvent {
                        renderer.nativeOrbit(dx, dy)
                    }
                } else if (event.pointerCount >= 2) {
                    // Zoom using 2-finger gesture if supported (vertical movement)
                    val dy = y - prevY
                    queueEvent {
                        renderer.nativeZoom(dy * 0.05f)
                    }
                }
                prevX = x
                prevY = y
            }
        }
        return true
    }
}
