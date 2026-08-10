// SPDX-License-Identifier: GPL-2.0
package org.subsurfacedivelog.mobile;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import org.qtproject.qt.android.QtNative;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class CloudCredentialStore {
    private static final String KEY_ALIAS = "SubsurfaceNeoCloudOAuth";
    private static final String PREFS = "subsurface_neo_cloud_credentials";
    private static final String TRANSFORMATION = "AES/GCM/NoPadding";

    private CloudCredentialStore() {}

    private static Context context() {
        return QtNative.getContext();
    }

    private static SecretKey getOrCreateKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        if (keyStore.containsAlias(KEY_ALIAS)) {
            return ((KeyStore.SecretKeyEntry) keyStore.getEntry(KEY_ALIAS, null)).getSecretKey();
        }

        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(
                KEY_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true)
                .build());
        return generator.generateKey();
    }

    public static boolean save(String providerId, String base64Payload) {
        try {
            Cipher cipher = Cipher.getInstance(TRANSFORMATION);
            cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey());
            byte[] encrypted = cipher.doFinal(base64Payload.getBytes(StandardCharsets.UTF_8));
            String iv = Base64.encodeToString(cipher.getIV(), Base64.NO_WRAP);
            String ciphertext = Base64.encodeToString(encrypted, Base64.NO_WRAP);
            return context().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .putString(providerId + ".iv", iv)
                    .putString(providerId + ".data", ciphertext)
                    .commit();
        } catch (Exception ignored) {
            return false;
        }
    }

    public static String load(String providerId) {
        try {
            SharedPreferences preferences = context().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            String iv = preferences.getString(providerId + ".iv", null);
            String ciphertext = preferences.getString(providerId + ".data", null);
            if (iv == null || ciphertext == null)
                return null;

            Cipher cipher = Cipher.getInstance(TRANSFORMATION);
            cipher.init(Cipher.DECRYPT_MODE, getOrCreateKey(),
                    new GCMParameterSpec(128, Base64.decode(iv, Base64.NO_WRAP)));
            byte[] clear = cipher.doFinal(Base64.decode(ciphertext, Base64.NO_WRAP));
            return new String(clear, StandardCharsets.UTF_8);
        } catch (Exception ignored) {
            return null;
        }
    }

    public static boolean remove(String providerId) {
        try {
            return context().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                    .edit()
                    .remove(providerId + ".iv")
                    .remove(providerId + ".data")
                    .commit();
        } catch (Exception ignored) {
            return false;
        }
    }
}
