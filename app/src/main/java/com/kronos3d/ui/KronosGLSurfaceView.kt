package com.kronos3d.ui

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import com.kronos3d.GLSurfaceManager

class KronosGLSurfaceView(context: Context) : GLSurfaceView(context) {
    val renderer: GLSurfaceManager

    init {
        setEGLContextClientVersion(3)
        
        // CRÍTICO: Permitir que Views de Android se rendericen encima
        setZOrderOnTop(false)
        setZOrderMediaOverlay(false)
        
        // Hacer el fondo transparente para compositing correcto
        setEGLConfigChooser(8, 8, 8, 8, 16, 0)
        holder.setFormat(android.graphics.PixelFormat.TRANSLUCENT)
        
        renderer = GLSurfaceManager()
        setRenderer(renderer)
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
                queueEvent {
                    renderer.nativeGizmoDown(x, y)
                }
            }
            MotionEvent.ACTION_UP -> {
                val duration = System.currentTimeMillis() - downTime
                val dist = Math.hypot((x - prevX).toDouble(), (y - prevY).toDouble())
                if (duration < 200 && dist < 15) {
                    queueEvent {
                        renderer.nativeTap(x, y)
                    }
                }
                queueEvent {
                    renderer.nativeGizmoUp()
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1) {
                    val dx = x - prevX
                    val dy = y - prevY
                    queueEvent {
                        renderer.nativeOrbit(dx, dy)
                    }
                } else if (event.pointerCount >= 2) {
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
