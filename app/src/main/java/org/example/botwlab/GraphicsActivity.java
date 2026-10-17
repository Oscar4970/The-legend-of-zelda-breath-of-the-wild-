package org.example.botwlab;

import android.app.Activity;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
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

/** Standalone 3D hardware test. NOT a Nintendo game, emulator or Switch runtime. */
public final class GraphicsActivity extends Activity {
    private GLSurfaceView surface;
    private final TerrainRenderer renderer = new TerrainRenderer();
    private int movePointer = -1, lookPointer = -1;
    private float moveOriginX, moveOriginY, lastLookX, lastLookY;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        surface = new GLSurfaceView(this);
        surface.setEGLContextClientVersion(3);
        surface.setRenderer(renderer);
        surface.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        surface.setOnTouchListener((view, event) -> onControl(event));
        FrameLayout layout = new FrameLayout(this);
        layout.addView(surface);
        TextView info = new TextView(this);
        info.setTextColor(0xffffffff);
        info.setBackgroundColor(0xa6000000);
        info.setPadding(18, 14, 18, 14);
        info.setText("Escenario 3D de PRUEBA — NO ES ZELDA");
        FrameLayout.LayoutParams top = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        layout.addView(info, top);
        TextView hints = new TextView(this);
        hints.setText("Lado izquierdo: caminar  •  Lado derecho: mover cámara");
        hints.setTextColor(0xffffffff);
        hints.setBackgroundColor(0x99000000);
        hints.setPadding(18, 12, 18, 12);
        FrameLayout.LayoutParams bottom = new FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        layout.addView(hints, bottom);
        renderer.status = text -> runOnUiThread(() -> info.setText(text));
        setContentView(layout);
    }

    private boolean onControl(MotionEvent event) {
        int action = event.getActionMasked();
        int changed = event.getActionIndex();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int id = event.getPointerId(changed);
            float x = event.getX(changed), y = event.getY(changed);
            if (x < surface.getWidth() * 0.42f && movePointer == -1) {
                movePointer = id;
                moveOriginX = x;
                moveOriginY = y;
            } else if (lookPointer == -1) {
                lookPointer = id;
                lastLookX = x;
                lastLookY = y;
            }
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            int i = event.findPointerIndex(movePointer);
            if (i >= 0) {
                float radius = Math.max(80f, Math.min(surface.getWidth(), surface.getHeight()) * 0.16f);
                renderer.strafe = clamp((event.getX(i) - moveOriginX) / radius, -1, 1);
                renderer.forward = clamp((moveOriginY - event.getY(i)) / radius, -1, 1);
            }
            i = event.findPointerIndex(lookPointer);
            if (i >= 0) {
                float x = event.getX(i), y = event.getY(i);
                renderer.yaw += (x - lastLookX) * 0.004f;
                renderer.pitch = clamp(renderer.pitch - (y - lastLookY) * 0.003f, -1.1f, 1.1f);
                lastLookX = x;
                lastLookY = y;
            }
            return true;
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL) {
            int id = (action == MotionEvent.ACTION_CANCEL) ? -100 : event.getPointerId(changed);
            if (id == movePointer || action == MotionEvent.ACTION_CANCEL) {
                movePointer = -1;
                renderer.strafe = renderer.forward = 0f;
            }
            if (id == lookPointer || action == MotionEvent.ACTION_CANCEL) lookPointer = -1;
            return true;
        }
        return true;
    }

    private static float clamp(float x, float lo, float hi) { return Math.max(lo, Math.min(hi, x)); }

    @Override protected void onPause() {
        if (surface != null) surface.onPause();
        renderer.strafe = renderer.forward = 0;
        super.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        if (surface != null) surface.onResume();
    }

    private static final class TerrainRenderer implements GLSurfaceView.Renderer {
        interface Status { void update(String msg); }
        volatile Status status;
        volatile float strafe, forward, yaw = 0.0f, pitch = -0.12f;
        private float camX = 0, camZ = 10;
        private int program, vbo, vertexCount, vpLocation;
        private int width, height;
        private long previousFrame, fpsStart;
        private int frameCount;
        private String gpu = "desconocida";
        private final float[] projection = new float[16], view = new float[16], vp = new float[16];

        private static float ground(float x, float z) {
            return 1.15f * (float)Math.sin(x * 0.16f) * (float)Math.cos(z * 0.13f)
                 + 0.42f * (float)Math.sin(z * 0.29f);
        }

        private static int shader(int kind, String code) {
            int s = GLES30.glCreateShader(kind);
            GLES30.glShaderSource(s, code);
            GLES30.glCompileShader(s);
            int[] ok = new int[1];
            GLES30.glGetShaderiv(s, GLES30.GL_COMPILE_STATUS, ok, 0);
            if (ok[0] != 1) throw new IllegalStateException("Shader: " + GLES30.glGetShaderInfoLog(s));
            return s;
        }

        @Override public void onSurfaceCreated(GL10 unused, EGLConfig config) {
            int vs = shader(GLES30.GL_VERTEX_SHADER,
                "#version 300 es\nlayout(location=0) in vec3 p; layout(location=1) in vec3 c; uniform mat4 uVP; out vec3 vC; void main(){gl_Position=uVP*vec4(p,1.);vC=c;}");
            int fs = shader(GLES30.GL_FRAGMENT_SHADER,
                "#version 300 es\nprecision mediump float;in vec3 vC;out vec4 o;void main(){o=vec4(vC,1.);}");
            program = GLES30.glCreateProgram();
            GLES30.glAttachShader(program,vs);
            GLES30.glAttachShader(program,fs);
            GLES30.glLinkProgram(program);
            int[] ok = new int[1];
            GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);
            if(ok[0] != 1) throw new IllegalStateException("Program: " + GLES30.glGetProgramInfoLog(program));
            GLES30.glDeleteShader(vs);
            GLES30.glDeleteShader(fs);
            vpLocation = GLES30.glGetUniformLocation(program,"uVP");
            Mesh mesh = new Mesh(200000);
            for (int z = -36; z < 36; z++) {
                for (int x = -36; x < 36; x++) {
                    float g = ((x + z) & 1) == 0 ? .45f : .51f;
                    float[] a={x,ground(x,z),z}, b={x+1,ground(x+1,z),z};
                    float[] c={x,ground(x,z+1),z+1}, d={x+1,ground(x+1,z+1),z+1};
                    mesh.tri(a,b,c,.15f,g,.27f);
                    mesh.tri(b,d,c,.15f,g+.03f,.27f);
                }
            }
            // Several freely modeled landmarks, not assets from any commercial game.
            for(int i=0;i<24;i++){
                float x=(i%6-2.5f)*10f, z=(i/6-1.5f)*12f;
                float elevation=ground(x,z), tall=2.3f+(i%4)*1.7f;
                mesh.box(x,elevation,z,2.0f,tall,2.0f,.54f,.51f,.42f);
            }
            float[] data = mesh.array();
            vertexCount = mesh.count / 6;
            FloatBuffer buffer = ByteBuffer.allocateDirect(mesh.count*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            buffer.put(data,0,mesh.count).position(0);
            int[] ids=new int[1];GLES30.glGenBuffers(1,ids,0);vbo=ids[0];
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER,vbo);
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER,mesh.count*4,buffer,GLES30.GL_STATIC_DRAW);
            GLES30.glEnable(GLES30.GL_DEPTH_TEST);
            GLES30.glDepthFunc(GLES30.GL_LEQUAL);
            gpu=GLES30.glGetString(GLES30.GL_RENDERER);
            previousFrame=fpsStart=System.nanoTime();
            frameCount=0;
        }

        @Override public void onSurfaceChanged(GL10 unused, int w, int h) {
            width=w;height=h;
            GLES30.glViewport(0,0,w,h);
            Matrix.perspectiveM(projection,0,65f,w/(float)Math.max(1,h),.1f,170f);
        }

        @Override public void onDrawFrame(GL10 unused) {
            long now=System.nanoTime();
            float dt=clamp((now-previousFrame)/1.0e9f,0,.05f);
            previousFrame=now;
            float moveForward=forward, moveSide=strafe, len=(float)Math.hypot(moveForward,moveSide);
            if (len>1f) {moveForward/=len;moveSide/=len;}
            float heading=yaw;
            camX += (moveForward*(float)Math.sin(heading)+moveSide*(float)Math.cos(heading))*dt*5.2f;
            camZ += (-moveForward*(float)Math.cos(heading)+moveSide*(float)Math.sin(heading))*dt*5.2f;
            camX=clamp(camX,-34,34);camZ=clamp(camZ,-34,34);
            float y=ground(camX,camZ)+2.2f;
            float cp=(float)Math.cos(pitch);
            Matrix.setLookAtM(view,0,camX,y,camZ,
                camX+cp*(float)Math.sin(yaw),y+(float)Math.sin(pitch),
                camZ-cp*(float)Math.cos(yaw),0f,1f,0f);
            Matrix.multiplyMM(vp,0,projection,0,view,0);
            GLES30.glClearColor(.36f,.60f,.82f,1f);
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT|GLES30.GL_DEPTH_BUFFER_BIT);
            GLES30.glUseProgram(program);
            GLES30.glUniformMatrix4fv(vpLocation,1,false,vp,0);
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER,vbo);
            GLES30.glEnableVertexAttribArray(0);
            GLES30.glVertexAttribPointer(0,3,GLES30.GL_FLOAT,false,24,0);
            GLES30.glEnableVertexAttribArray(1);
            GLES30.glVertexAttribPointer(1,3,GLES30.GL_FLOAT,false,24,12);
            GLES30.glDrawArrays(GLES30.GL_TRIANGLES,0,vertexCount);
            frameCount++;
            if(now-fpsStart>=1000000000L){
                int fps=(int)Math.round(frameCount*1.0e9/(now-fpsStart));
                Status s=status;
                if(s!=null)s.update("PRUEBA 3D — NO ES ZELDA  •  "+fps+" FPS\n"+gpu+"\n"+
                                    width+"×"+height+"  •  "+(vertexCount/3)+" triángulos");
                fpsStart=now;frameCount=0;
            }
        }

        private static final class Mesh {
            float[] a; int count;
            Mesh(int capacity){a=new float[capacity];}
            float[] array(){return a;}
            void vertex(float x,float y,float z,float r,float g,float b){
                if(count+6>a.length)throw new IllegalStateException("Mesh buffer full");
                a[count++]=x;a[count++]=y;a[count++]=z;
                a[count++]=r;a[count++]=g;a[count++]=b;
            }
            void tri(float[] p,float[] q,float[] r,float red,float green,float blue) {
                vertex(p[0],p[1],p[2],red,green,blue);
                vertex(q[0],q[1],q[2],red,green,blue);
                vertex(r[0],r[1],r[2],red,green,blue);
            }
            void quad(float[] a,float[] b,float[] c,float[] d,float r,float g,float bl) {
                tri(a,b,c,r,g,bl);tri(a,c,d,r,g,bl);
            }
            void box(float x,float y,float z,float w,float h,float dep,float r,float g,float b) {
                float xx=x+w,yy=y+h,zz=z+dep;
                float[] p={x,y,z},q={xx,y,z},s={xx,yy,z},t={x,yy,z};
                float[] u={x,y,zz},v={xx,y,zz},a={xx,yy,zz},c={x,yy,zz};
                quad(p,q,s,t,r*.85f,g*.85f,b*.85f);
                quad(v,u,c,a,r,g,b);
                quad(u,p,t,c,r*.70f,g*.70f,b*.70f);
                quad(q,v,a,s,r*.78f,g*.78f,b*.78f);
                quad(t,s,a,c,r*1.08f,g*1.08f,b*1.08f);
            }
        }
    }
}
