// SPDX-License-Identifier: GPL-2.0
package org.subsurfacedivelog.mobile;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;

public final class NeoUpdateNotifier {
    private NeoUpdateNotifier() {}

    public static void show(final String version, final String url) {
        final Activity activity = SubsurfaceMobileActivity.getCurrentActivity();
        if (activity == null || activity.isFinishing())
            return;

        activity.runOnUiThread(() -> {
            if (activity.isFinishing())
                return;
            new AlertDialog.Builder(activity)
                    .setTitle("Subsurface Neo update")
                    .setMessage("A new version is available (" + version + ").")
                    .setNegativeButton("Later", null)
                    .setPositiveButton("Download", (dialog, which) -> {
                        try {
                            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                            activity.startActivity(intent);
                        } catch (Exception ignored) {
                        }
                    })
                    .show();
        });
    }
}
