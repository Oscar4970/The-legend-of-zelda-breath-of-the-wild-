package org.example.botwlab;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.FeatureInfo;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class MainActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        ScrollView scroll = new ScrollView(this);
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
        text.append("\nEl indicador Vulkan no confirma que el juego funcione.");
        info.setText(text.toString());
        info.setTextIsSelectable(true);
        body.addView(info);
        Button test = new Button(this);
        test.setText("Probar gráficos y pantalla táctil");
        test.setOnClickListener(v -> startActivity(new Intent(this, GraphicsActivity.class)));
        body.addView(test);
        scroll.addView(body);
        setContentView(scroll);
    }
}
