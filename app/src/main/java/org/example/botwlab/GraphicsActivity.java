package org.example.botwlab;

import android.app.Activity;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class GraphicsActivity extends Activity {
    private GLSurfaceView surface;
    private final ShapeRenderer renderer = new ShapeRenderer();
    private float touchX;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        surface = new GLSurfaceView(this);
        surface.setEGLContextClientVersion(3);
        surface.setRenderer(renderer);
        surface.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        surface.setOnTouchListener((view, event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: touchX = event.getX(); return true;
                case MotionEvent.ACTION_MOVE:
                    renderer.rotation += (event.getX() - touchX) / 280f;
                    touchX = event.getX();
                    return true;
                case MotionEvent.ACTION_UP: return true;
                default: return false;
            }
        });
        FrameLayout frame = new FrameLayout(this);
        frame.addView(surface);
        TextView label = new TextView(this);
        label.setTextColor(0xffffffff);
        label.setBackgroundColor(0xaa000000);
        label.setPadding(18, 16, 18, 16);
        label.setText("Prueba gráfica Android — NO ES ZELDA\nDesliza para girar el triángulo");
        frame.addView(label, new FrameLayout.LayoutParams(-1, -2, Gravity.TOP));
        renderer.onStatus = msg -> runOnUiThread(() -> label.setText(msg));
        setContentView(frame);
    }

    @Override protected void onPause() { surface.onPause(); super.onPause(); }
    @Override protected void onResume() { super.onResume(); surface.onResume(); }

    private static final class ShapeRenderer implements GLSurfaceView.Renderer {
        interface Status { void update(String msg); }
        volatile Status onStatus;
        volatile float rotation;
        private int program;
        private int vbo;
        private int uRotation;
        private long startNs;
        private int frames;
        private String gpu = "desconocida";
        private int width, height;

        private static int compile(int kind, String code) {
            int shader = GLES30.glCreateShader(kind);
            GLES30.glShaderSource(shader, code);
            GLES30.glCompileShader(shader);
            int[] result = new int[1];
            GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, result, 0);
            if (result[0] == 0) throw new IllegalStateException(GLES30.glGetShaderInfoLog(shader));
            return shader;
        }

        @Override public void onSurfaceCreated(GL10 ignored, EGLConfig config) {
            int vertex = compile(GLES30.GL_VERTEX_SHADER,
                "#version 300 es\nlayout(location=0) in vec2 pos; layout(location=1) in vec3 col; uniform float angle; out vec3 color; void main(){float c=cos(angle),s=sin(angle);gl_Position=vec4(pos.x*c-pos.y*s,pos.x*s+pos.y*c,0.,1.);color=col;}");
            int fragment = compile(GLES30.GL_FRAGMENT_SHADER,
                "#version 300 es\nprecision mediump float; in vec3 color; out vec4 finalColor; void main(){finalColor=vec4(color,1.);}");
            program = GLES30.glCreateProgram();
            GLES30.glAttachShader(program, vertex);
            GLES30.glAttachShader(program, fragment);
            GLES30.glLinkProgram(program);
            int[] linked = new int[1];
            GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, linked, 0);
            if (linked[0] == 0) throw new IllegalStateException(GLES30.glGetProgramInfoLog(program));
            GLES30.glDeleteShader(vertex);
            GLES30.glDeleteShader(fragment);
            uRotation = GLES30.glGetUniformLocation(program, "angle");
            float[] vertices = {-0.55f,-0.65f,0.15f,0.65f,0.85f, 0.55f,-0.65f,0.20f,0.85f,0.50f, 0.00f,0.65f,0.90f,0.55f,0.20f};
            FloatBuffer buffer = ByteBuffer.allocateDirect(vertices.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            buffer.put(vertices).position(0);
            int[] ids = new int[1];
            GLES30.glGenBuffers(1, ids, 0);
            vbo = ids[0];
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo);
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, vertices.length * 4, buffer, GLES30.GL_STATIC_DRAW);
            gpu = GLES30.glGetString(GLES30.GL_RENDERER);
            startNs = System.nanoTime();
        }

        @Override public void onSurfaceChanged(GL10 ignored, int w, int h) {
            width = w; height = h;
            GLES30.glViewport(0, 0, w, h);
        }

        @Override public void onDrawFrame(GL10 ignored) {
            GLES30.glClearColor(0.06f,0.12f,0.20f,1f);
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
            GLES30.glUseProgram(program);
            GLES30.glUniform1f(uRotation, rotation);
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo);
            GLES30.glEnableVertexAttribArray(0);
            GLES30.glVertexAttribPointer(0, 2, GLES30.GL_FLOAT, false, 20, 0);
            GLES30.glEnableVertexAttribArray(1);
            GLES30.glVertexAttribPointer(1, 3, GLES30.GL_FLOAT, false, 20, 8);
            GLES30.glDrawArrays(GLES30.GL_TRIANGLES, 0, 3);
            frames++;
            long now = System.nanoTime();
            if (now - startNs >= 1000000000L) {
                int fps = (int)Math.round(frames * 1.0e9/(now - startNs));
                Status callback = onStatus;
                if (callback != null) callback.update("Gráficos Android: " + gpu + "\nFPS de PRUEBA: " + fps + " | " + width + "×" + height + "\nNO ES ZELDA — arrastra para girar");
                startNs = now; frames = 0;
            }
        }
    }
}
