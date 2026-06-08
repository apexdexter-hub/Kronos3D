package com.kronos3d

import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GLSurfaceManager : GLSurfaceView.Renderer {

    companion object {
        init {
            System.loadLibrary("kronos3d_engine")
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        nativeInit()
        nativeLoadMesh()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        nativeRender()
    }

    // JNI Native methods
    private external fun nativeInit()
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()
    
    // Touch handler delegates
    external fun nativeOrbit(dx: Float, dy: Float)
    external fun nativeZoom(delta: Float)
    external fun nativeTap(pixelX: Float, pixelY: Float)

    // Kotlin UI control & HUD query bridges
    external fun nativeToggleEditMode()
    external fun nativeSetToolSelect()
    external fun nativeSetToolMove()
    external fun nativeSetToolRotate()
    external fun nativeSetToolScale()
    external fun nativeExtrude()
    external fun nativeSubdivide()
    external fun nativeUndo()
    external fun nativeGetFPS(): Float
    external fun nativeGetVertCount(): Int
    external fun nativeGetFaceCount(): Int
    external fun nativeSaveMesh()
    external fun nativeLoadMesh()
    external fun nativeGizmoDown(x: Float, y: Float)
    external fun nativeGizmoUp()
    external fun nativeCycleLightColor()
}
