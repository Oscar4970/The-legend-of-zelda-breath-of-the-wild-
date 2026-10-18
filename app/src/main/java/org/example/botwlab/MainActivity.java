package org.example.botwlab;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.content.pm.FeatureInfo;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final int REQUEST_NSO = 73;
    private TextView scanResult;
    private static native String nativeVulkanInfo();
    private static native String nativeVulkanRuntimeProbe();
    private static native String nativeOffscreenGpuCheck();
    private static native String nativeTriangleGpuCheck();
    private static native String nativeRunRecompiledArm64();
    private static native String nativeRunCompiledHomebrew();
    private static native String nativeInspectSwitchContainer(int fd);
    private static native String nativeInspectNso(int fd);
    private static native String nativeReconstructNso(int fd);
    private static native String nativeRunSyntheticArm64();
    private static native String nativeRunLinkedArm64();
    private static native String nativeInspectMod0(int fd);
    private static native String nativeInspectNro(int fd);
    private static native String nativeSimulateRelr(int fd);
    private static native String nativeAuditNroRuntime(int fd);
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        ScrollView scroll = new ScrollView(this);
        scroll.setFitsSystemWindows(true);
        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setGravity(Gravity.CENTER_HORIZONTAL);
        body.setPadding(32, 40, 32, 32);
        TextView heading = new TextView(this);
        heading.setText("BOTW Android — prueba de hardware");
        heading.setTextSize(24);
        body.addView(heading);
        TextView note = new TextView(this);
        note.setText("ESTO NO ES EL JUEGO. Prueba gráficos y controles táctiles; no abre ROMs ni ejecuta Zelda.");
        note.setPadding(0, 25, 0, 25);
        body.addView(note);
        TextView info = new TextView(this);
        StringBuilder text = new StringBuilder();
        text.append("Modelo: ").append(Build.MANUFACTURER).append(" ").append(Build.MODEL).append("\n");
        text.append("Android API: ").append(Build.VERSION.SDK_INT).append("\n");
        text.append("Arquitectura: ").append(Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "desconocida").append("\n");
        boolean vulkan = false;
        for (FeatureInfo f : getPackageManager().getSystemAvailableFeatures()) {
            if ("android.hardware.vulkan.level".equals(f.name)) {
                vulkan = true;
                text.append("Vulkan declarado: nivel ").append(f.version).append("\n");
            }
        }
        if (!vulkan) text.append("Vulkan: no declarado por PackageManager\n");
        text.append("\n");
        try {
            System.loadLibrary("botwgpu");
            text.append(nativeVulkanInfo());
        } catch (Throwable error) {
            text.append("Vulkan nativo no disponible: ").append(error.getClass().getSimpleName());
        }
        text.append("\n\nEsta prueba NO mide FPS del juego.");
        info.setText(text.toString());
        info.setTextIsSelectable(true);
        body.addView(info);
        Button test = new Button(this);
        test.setText("Probar gráficos y pantalla táctil");
        test.setOnClickListener(v -> startActivity(new Intent(this, GraphicsActivity.class)));
        body.addView(test);
        Button gpuProbe = new Button(this);
        gpuProbe.setText("Diagnosticar compatibilidad gráfica Vulkan para Switch");
        gpuProbe.setOnClickListener(v -> {
            try {
                System.loadLibrary("botwgpu");
                StringBuilder report = new StringBuilder(nativeVulkanRuntimeProbe());
                ActivityManager manager = (ActivityManager)getSystemService(ACTIVITY_SERVICE);
                if (manager != null) {
                    ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
                    manager.getMemoryInfo(memory);
                    report.append("\nRAM total Android: ")
                          .append(memory.totalMem / (1024L * 1024L))
                          .append(" MiB");
                    report.append("\nRAM disponible aprox.: ")
                          .append(memory.availMem / (1024L * 1024L))
                          .append(" MiB (cambia constantemente)");
                    report.append("\nMemoria baja del sistema: ")
                          .append(memory.lowMemory ? "si" : "no");
                }
                scanResult.setText(report.toString());
            } catch (Throwable error) {
                scanResult.setText("Diagnostico Vulkan no disponible: " +
                    error.getClass().getSimpleName() +
                    ". No se modificaron los ajustes del telefono.");
            }
        });
        body.addView(gpuProbe);
        Button gpuDraw = new Button(this);
        gpuDraw.setText("Probar GPU Vulkan real: dibujar color y verificar píxeles");
        gpuDraw.setOnClickListener(v -> {
            gpuDraw.setEnabled(false);
            scanResult.setText("Probando dibujo Vulkan 16x16 en GPU... (sin ROM ni shaders)");
            new Thread(() -> {
                String result;
                try {
                    System.loadLibrary("botwgpu");
                    result = nativeOffscreenGpuCheck();
                } catch (Throwable error) {
                    result = "Error al iniciar el test gráfico: " +
                        error.getClass().getSimpleName();
                }
                final String resultText = result;
                runOnUiThread(() -> {
                    scanResult.setText(resultText);
                    gpuDraw.setEnabled(true);
                });
            }, "VulkanGpuReadback").start();
        });
        body.addView(gpuDraw);
        Button triangle = new Button(this);
        triangle.setText("Probar TRIANGULO REAL con shaders SPIR-V y Vulkan");
        triangle.setOnClickListener(v -> {
            triangle.setEnabled(false);
            scanResult.setText("Dibujando triangulo de 64x64 con GPU Vulkan...");
            new Thread(() -> {
                String report;
                try {
                    System.loadLibrary("botwgpu");
                    report=nativeTriangleGpuCheck();
                } catch (Throwable ex) {
                    report="No fue posible iniciar el test SPIR-V: "+ex.getClass().getSimpleName();
                }
                final String message=report;
                runOnUiThread(() -> {
                    scanResult.setText(message);
                    triangle.setEnabled(true);
                });
            }, "VulkanSpirvTriangle").start();
        });
        body.addView(triangle);
        Button recompTest=new Button(this);
        recompTest.setText("Ejecutar código ARM64 RECOMPILADO REAL (muestra propia)");
        recompTest.setOnClickListener(v -> {
            try {
                System.loadLibrary("botwgpu");
                scanResult.setText(nativeRunRecompiledArm64());
            } catch (Throwable error) {
                scanResult.setText("Recompilacion no disponible: "+
                    error.getClass().getSimpleName());
            }
        });
        body.addView(recompTest);
        Button programTest = new Button(this);
        programTest.setText("Probar PROGRAMA C ARM64 RECOMPILADO (funciones, memoria y servicios)");
        programTest.setOnClickListener(v -> {
            programTest.setEnabled(false);
            try {
                System.loadLibrary("botwgpu");
                scanResult.setText(nativeRunCompiledHomebrew());
            } catch (Throwable error) {
                scanResult.setText("Programa recompilado no disponible: " +
                    error.getClass().getSimpleName());
            } finally {
                programTest.setEnabled(true);
            }
        });
        body.addView(programTest);
        Button romInspector = new Button(this);
        romInspector.setText("INSPECCIONAR ARCHIVO NSP/XCI LOCAL (SOLO INDICE, NO EJECUTA)");
        romInspector.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick,79);
        });
        body.addView(romInspector);
        Button select = new Button(this);
        select.setText("Analizar NSO de Switch (solo metadatos)");
        select.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, REQUEST_NSO);
        });
        body.addView(select);
        Button reconstruct = new Button(this);
        reconstruct.setText("Reconstruir imagen NSO (máx. 16 MiB, no ejecuta)");
        reconstruct.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, 74);
        });
        body.addView(reconstruct);
        Button mod0 = new Button(this);
        mod0.setText("Inspeccionar MOD0, bibliotecas y enlaces de Switch");
        mod0.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, 75);
        });
        body.addView(mod0);
        Button nro = new Button(this);
        nro.setText("Inspeccionar homebrew NRO de Switch (solo lectura)");
        nro.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, 76);
        });
        body.addView(nro);
        Button simulate = new Button(this);
        simulate.setText("Simular RELR en copia de NRO (NO ejecuta)");
        simulate.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, 77);
        });
        body.addView(simulate);
        Button audit = new Button(this);
        audit.setText("Analizar requisitos para ejecutar NRO real (no ejecuta)");
        audit.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, 78);
        });
        body.addView(audit);
        Button execute = new Button(this);
        execute.setText("Ejecutar función ARM64 ficticia (19 + 23)");
        execute.setOnClickListener(v -> {
            try {
                System.loadLibrary("botwgpu");
                scanResult.setText(nativeRunSyntheticArm64());
            } catch (Throwable error) {
                scanResult.setText("La prueba ARM64 no se pudo iniciar: " + error.getClass().getSimpleName());
            }
        });
        body.addView(execute);
        Button linked = new Button(this);
        linked.setText("Probar enlace ARM64: dos funciones y dato externo");
        linked.setOnClickListener(v -> {
            try {
                System.loadLibrary("botwgpu");
                scanResult.setText(nativeRunLinkedArm64());
            } catch (Throwable error) {
                scanResult.setText("No se pudo iniciar la prueba de enlaces: " + error.getClass().getSimpleName());
            }
        });
        body.addView(linked);
        scanResult = new TextView(this);
        scanResult.setPadding(0, 18, 0, 18);
        scanResult.setText("Selecciona un NSO0 sin cifrar de una copia propia. No ejecuta el archivo.");
        scanResult.setTextIsSelectable(true);
        body.addView(scanResult);
        scroll.addView(body);
        setContentView(scroll);
    }
    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 79) {
            if(resultCode != RESULT_OK || data == null || data.getData() == null) return;
            final Uri selected=data.getData();
            scanResult.setText("Inspeccionando solo cabecera e indice del contenedor...\n"
                    +"No se cargara el juego completo ni se enviara a internet.");
            new Thread(() -> {
                String result;
                try (ParcelFileDescriptor fd=getContentResolver().openFileDescriptor(selected,"r")) {
                    if(fd == null) throw new IllegalStateException("No se pudo abrir el archivo.");
                    System.loadLibrary("botwgpu");
                    result=nativeInspectSwitchContainer(fd.getFd());
                } catch (Throwable error) {
                    result="Error leyendo archivo NSP/XCI: "+error.getClass().getSimpleName()
                            +": "+String.valueOf(error.getMessage());
                }
                final String value=result;
                runOnUiThread(() -> scanResult.setText(value));
            },"NspXciMetadataOnly").start();
            return;
        }
        if ((requestCode != REQUEST_NSO && requestCode != 74 && requestCode != 75 && requestCode != 76 && requestCode != 77 && requestCode != 78) || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        try (ParcelFileDescriptor descriptor = getContentResolver().openFileDescriptor(uri, "r")) {
            if (descriptor == null) throw new IllegalStateException("No fue posible abrir el archivo.");
            System.loadLibrary("botwgpu");
            scanResult.setText(requestCode == 78
                ? nativeAuditNroRuntime(descriptor.getFd())
                : requestCode == 77
                  ? nativeSimulateRelr(descriptor.getFd())
                  : requestCode == 76
                  ? nativeInspectNro(descriptor.getFd())
                  : requestCode == 75
                  ? nativeInspectMod0(descriptor.getFd())
                  : requestCode == 74
                  ? nativeReconstructNso(descriptor.getFd())
                  : nativeInspectNso(descriptor.getFd()));
        } catch (Exception | LinkageError error) {
            scanResult.setText("No se pudo analizar el archivo: " + error.getClass().getSimpleName());
        }
    }
}
