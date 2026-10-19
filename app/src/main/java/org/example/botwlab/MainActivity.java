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
import java.io.File;
import java.io.InputStream;
import java.util.zip.ZipFile;

public final class MainActivity extends Activity {
    private static final int REQUEST_NSO = 73;
    private static final int REQUEST_NCA_BRIDGE_NSP = 81;
    private static final int REQUEST_NCA_BRIDGE_KEYS = 82;
    private static final int REQUEST_FIRMWARE_BRIDGE_ZIP = 83;
    private static final int REQUEST_FIRMWARE_BRIDGE_KEYS = 84;
    private Uri firmwareBridgeZipUri;
    private Uri bridgeNspUri;
    // No Nintendo-derived game library is distributed in this repository.
    private boolean hasPrivateGameAndData() {
        boolean hasLibrary;
        try (ZipFile file = new ZipFile(getApplicationInfo().sourceDir)) {
            hasLibrary = file.getEntry("lib/arm64-v8a/libbotw.so") != null;
        } catch (Exception ignored) { return false; }
        File data = getExternalFilesDir(null);
        return hasLibrary && data != null
            && new File(data, "analysis/main.bin").isFile()
            && new File(data, "analysis/sdk.bin").isFile()
            && new File(data, "extract/romfs").isDirectory();
    }
    private TextView scanResult;
    private Button shareRomReport;
    private String lastRomReport = "";
    private static native String nativeVulkanInfo();
    private static native String nativeVulkanRuntimeProbe();
    private static native String nativeOffscreenGpuCheck();
    private static native String nativeTriangleGpuCheck();
    private static native String nativeRunRecompiledArm64();
    private static native String nativeRunCompiledHomebrew();
    private static native String nativeInspectSwitchContainer(int fd);
    private static native String nativeAnalyzeSwitchDeep(int fd);
    private static native byte[][] nativeNcaHeaderSamples(int fd);
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
        heading.setText("BOTW Android — port nativo de Switch");
        heading.setTextSize(24);
        body.addView(heading);
        TextView note = new TextView(this);
        note.setText("Motor nativo en desarrollo. Este APK publico no incluye ejecutables, shaders ni recursos de Nintendo. El port aun no es jugable.");
        note.setPadding(0, 25, 0, 25);
        body.addView(note);
        boolean privateGameReady = hasPrivateGameAndData();
        Button launchGame = new Button(this);
        launchGame.setText(privateGameReady
            ? "INTENTAR INICIAR BOTW (BUILD PRIVADO EXPERIMENTAL)"
            : "BOTW NO DISPONIBLE: faltan codigo nativo y recursos");
        launchGame.setEnabled(privateGameReady);
        launchGame.setOnClickListener(v ->
            startActivity(new Intent(this, android.app.NativeActivity.class)));
        body.addView(launchGame);
        TextView gameStatus = new TextView(this);
        gameStatus.setPadding(0, 4, 0, 16);
        gameStatus.setText(privateGameReady
            ? "Biblioteca y datos detectados. Su presencia no garantiza arranque; Horizon y NVN pueden fallar."
            : "El motor privado requiere archivos legitimos, recompilacion de BOTW y datos locales. Revisa port_botw/README.md.");
        body.addView(gameStatus);
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
        programTest.setText("Probar SVC REALES de Switch en C recompilado (sm: abrir/cerrar)");
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
        romInspector.setText("ANALIZAR ROM A FONDO: NSP, XCI, NSO, NRO (solo lectura)");
        romInspector.setOnClickListener(v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick,79);
        });
        body.addView(romInspector);
        Button ncaBridge = new Button(this);
        ncaBridge.setText("PUENTE EDEN: comprobar NSP + prod.keys local (cabeceras NCA)");
        ncaBridge.setOnClickListener(v -> {
            bridgeNspUri = null;
            shareRomReport.setEnabled(false);
            lastRomReport = "";
            scanResult.setText("1/2: selecciona el NSP. Después elegirás prod.keys localmente. "
                + "No se guardan claves ni se envían a ningún servidor.");
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, REQUEST_NCA_BRIDGE_NSP);
        });
        body.addView(ncaBridge);
        Button firmwareBridge = new Button(this);
        firmwareBridge.setText("PUENTE FIRMWARE: inspeccionar ZIP + prod.keys localmente (NO EJECUTA)");
        firmwareBridge.setOnClickListener(v -> {
            firmwareBridgeZipUri = null;
            shareRomReport.setEnabled(false);
            lastRomReport = "";
            scanResult.setText("1/2: selecciona el ZIP del firmware de Switch. "
                + "La app no copiará ni instalará firmware.");
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, REQUEST_FIRMWARE_BRIDGE_ZIP);
        });
        body.addView(firmwareBridge);
        shareRomReport = new Button(this);
        shareRomReport.setText("COMPARTIR INFORME DE ANALISIS (solo texto)");
        shareRomReport.setEnabled(false);
        shareRomReport.setOnClickListener(v -> {
            if(lastRomReport.isEmpty()) return;
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("text/plain");
            send.putExtra(Intent.EXTRA_TEXT, lastRomReport);
            startActivity(Intent.createChooser(send, "Compartir reporte de ROM"));
        });
        body.addView(shareRomReport);
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
        if (requestCode == REQUEST_FIRMWARE_BRIDGE_ZIP) {
            if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
            firmwareBridgeZipUri = data.getData();
            scanResult.setText("2/2: selecciona prod.keys desde tu teléfono. "
                + "Se utilizará solo en memoria.");
            Intent keys = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            keys.addCategory(Intent.CATEGORY_OPENABLE);
            keys.setType("*/*");
            startActivityForResult(keys, REQUEST_FIRMWARE_BRIDGE_KEYS);
            return;
        }
        if (requestCode == REQUEST_FIRMWARE_BRIDGE_KEYS) {
            if (resultCode != RESULT_OK || data == null || data.getData() == null ||
                firmwareBridgeZipUri == null) return;
            final Uri firmwareUri = firmwareBridgeZipUri;
            firmwareBridgeZipUri = null;
            final Uri keysUri = data.getData();
            scanResult.setText("Inspeccionando NCA dentro del firmware ZIP, localmente... "
                + "El archivo puede tardar; se lee sin extraerlo.");
            new Thread(() -> {
                String report;
                try (InputStream zipFile = getContentResolver().openInputStream(firmwareUri);
                     InputStream keysFile = getContentResolver().openInputStream(keysUri)) {
                    report = SwitchFirmwareBridge.inspect(zipFile, keysFile);
                } catch (Exception error) {
                    report = "PUENTE FIRMWARE: análisis no completado ("
                        + error.getClass().getSimpleName()
                        + "). Comprueba que seleccionaste el ZIP correcto y un prod.keys local. "
                        + "No se modificó el firmware ni se enviaron claves.";
                }
                final String value = report;
                runOnUiThread(() -> {
                    scanResult.setText(value);
                    lastRomReport = value;
                    shareRomReport.setEnabled(true);
                });
            }, "BotwLocalFirmwareInventory").start();
            return;
        }
        if (requestCode == REQUEST_NCA_BRIDGE_NSP) {
            if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
            bridgeNspUri = data.getData();
            scanResult.setText("2/2: elige tu prod.keys local. No lo incluiremos en el APK "
                + "ni se almacenará ni enviará a Internet.");
            Intent choose = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            choose.addCategory(Intent.CATEGORY_OPENABLE);
            choose.setType("*/*");
            startActivityForResult(choose, REQUEST_NCA_BRIDGE_KEYS);
            return;
        }
        if (requestCode == REQUEST_NCA_BRIDGE_KEYS) {
            if (bridgeNspUri == null || resultCode != RESULT_OK ||
                data == null || data.getData() == null) return;
            final Uri nsp = bridgeNspUri;
            bridgeNspUri = null;
            final Uri keyFile = data.getData();
            scanResult.setText("Comprobando cabeceras NCA localmente; sin extraer el juego...");
            new Thread(() -> {
                String report;
                try (ParcelFileDescriptor nspFd =
                         getContentResolver().openFileDescriptor(nsp, "r");
                     InputStream keys = getContentResolver().openInputStream(keyFile)) {
                    if (nspFd == null || keys == null)
                        throw new java.io.IOException("No se pudo abrir alguno de los archivos.");
                    System.loadLibrary("botwgpu");
                    byte[][] samples = nativeNcaHeaderSamples(nspFd.getFd());
                    report = SwitchNcaHeaderBridge.inspect(keys, samples);
                    // Ciphertext samples contain no secret key, but discard transient buffers.
                    for (byte[] sample : samples)
                        java.util.Arrays.fill(sample, (byte)0);
                } catch (Exception | LinkageError error) {
                    // Avoid accidentally showing full file paths or key bytes in reports.
                    report = "PUENTE NCA: comprobación no completada ("
                        + error.getClass().getSimpleName() + "). "
                        + "Revisa que seleccionaste un NSP y un archivo local con header_key. "
                        + "No se modificó el juego ni se exportaron claves.";
                }
                final String value = report;
                runOnUiThread(() -> {
                    scanResult.setText(value);
                    lastRomReport = value; // Metadata only, never private key bytes.
                    shareRomReport.setEnabled(true);
                });
            }, "BotwPrivateNcaHeaderProbe").start();
            return;
        }
        if (requestCode == 79) {
            if(resultCode != RESULT_OK || data == null || data.getData() == null) return;
            final Uri selected=data.getData();
            scanResult.setText("Analizando estructura del contenedor, particiones y ejecutables visibles...\n"
                    +"No se cargara el juego completo ni se enviara a internet.");
            new Thread(() -> {
                String result;
                try (ParcelFileDescriptor fd=getContentResolver().openFileDescriptor(selected,"r")) {
                    if(fd == null) throw new IllegalStateException("No se pudo abrir el archivo.");
                    System.loadLibrary("botwgpu");
                    result=nativeAnalyzeSwitchDeep(fd.getFd());
                } catch (Throwable error) {
                    result="Error leyendo archivo NSP/XCI: "+error.getClass().getSimpleName()
                            +": "+String.valueOf(error.getMessage());
                }
                final String value=result;
                runOnUiThread(() -> {
                    scanResult.setText(value);
                    lastRomReport = value;
                    shareRomReport.setEnabled(true);
                });
            },"SwitchDeepRomAnalysis").start();
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
