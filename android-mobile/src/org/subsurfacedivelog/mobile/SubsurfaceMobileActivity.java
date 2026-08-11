// SPDX-License-Identifier: GPL-2.0

package org.subsurfacedivelog.mobile;

import org.qtproject.qt.android.bindings.QtActivity;
import android.os.*;
import android.content.*;
import android.app.*;

import java.lang.String;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.util.Log;
import java.io.File;
import android.net.Uri;
import androidx.core.content.FileProvider;
import androidx.core.app.ShareCompat;
import java.util.ArrayList;

public class SubsurfaceMobileActivity extends QtActivity
{
	private static String fileProviderAuthority="org.subsurfacedivelog.mobile.fileprovider";

	public boolean shareViaEmail(String subject, String recipient, String body, String path1, String path2) {
		Log.d(TAG + " shareFile - trying to share: ", path1 + " and " + path2 + " to " + recipient);
		Intent shareFileIntent = new ShareCompat.IntentBuilder(this).getIntent();
		shareFileIntent.setAction(Intent.ACTION_SEND_MULTIPLE);
		shareFileIntent.putExtra(Intent.EXTRA_EMAIL, new String[] { recipient });
		shareFileIntent.putExtra(Intent.EXTRA_SUBJECT, subject);
		shareFileIntent.putExtra(Intent.EXTRA_TEXT, body);

		File fileToShare = new File(path1);
		Uri uri;
		try {
			uri = FileProvider.getUriForFile(this, fileProviderAuthority, fileToShare);
		} catch (IllegalArgumentException e) {
			Log.d(TAG + " shareFile - cannot get URI for ", path1);
			return false;
		}

		ArrayList<Uri> attachments = new ArrayList<Uri>();
		attachments.add(uri);
		if (!path2.isEmpty()) {
			fileToShare = new File(path2);
			try {
				uri = FileProvider.getUriForFile(this, fileProviderAuthority, fileToShare);
			} catch (IllegalArgumentException e) {
				Log.d(TAG + " shareFile - cannot get URI for ", path2);
				return false;
			}
			attachments.add(uri);
		}
		shareFileIntent.setType("text/plain");
		shareFileIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, attachments);
		shareFileIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
		shareFileIntent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
		this.startActivity(shareFileIntent);
		return true;
	}

	public boolean supportEmail(String path1, String path2) {
		return shareViaEmail("Subsurface-mobile support request",
				"in-app-support@subsurface-divelog.org",
				"Please describe your issue here and keep the attached logs.\n\n\n\n",
				path1, path2);
	}

	public static boolean isIntentPending;
	public static boolean isInitialized;
	private static String pendingOAuthUrl;
	private static final String TAG = "subsurfacedivelog.mobile";
	public static native void setUsbDevice(UsbDevice usbDevice);
	public static native void restartDownload(UsbDevice usbDevice);
	public static native void oauthCallback(String url);
	private static Context appContext;
	private static SubsurfaceMobileActivity currentActivity;

	private static boolean isOAuthIntent(Intent intent)
	{
		Uri data = intent == null ? null : intent.getData();
		return data != null && "subsurface-neo".equals(data.getScheme()) && "oauth".equals(data.getHost());
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		Log.i(TAG + " onCreate", "onCreate SubsurfaceMobileActivity");
		super.onCreate(savedInstanceState);
		androidx.core.view.WindowCompat.setDecorFitsSystemWindows(getWindow(), true);
		appContext = getApplicationContext();
		currentActivity = this;

		Intent theIntent = getIntent();
		if (isOAuthIntent(theIntent)) {
			pendingOAuthUrl = theIntent.getDataString();
			Log.i(TAG + " onCreate", "OAuth callback pending");
		} else if (theIntent != null && theIntent.getAction() != null) {
			Log.i(TAG + " onCreate", theIntent.getAction());
			isIntentPending = true;
		}

		IntentFilter filter = new IntentFilter("org.subsurfacedivelog.mobile.USB_PERMISSION");
		registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
	}

	@Override
	protected void onDestroy()
	{
		if (currentActivity == this)
			currentActivity = null;
		super.onDestroy();
	}

	@Override
	public void onNewIntent(Intent intent)
	{
		Log.i(TAG + " onNewIntent", String.valueOf(intent.getAction()));
		super.onNewIntent(intent);
		setIntent(intent);

		if (isOAuthIntent(intent)) {
			String callback = intent.getDataString();
			if (isInitialized) {
				oauthCallback(callback);
			} else {
				pendingOAuthUrl = callback;
			}
			return;
		}

		UsbDevice device = getUsbDevice(intent);
		if (device == null) {
			Log.i(TAG + " onNewIntent", "null device");
			return;
		}
		if (isInitialized)
			processIntent();
		else
			isIntentPending = true;
	}

	public void checkPendingIntents()
	{
		isInitialized = true;
		if (pendingOAuthUrl != null) {
			String callback = pendingOAuthUrl;
			pendingOAuthUrl = null;
			Log.i(TAG + " checkPendingIntents", "processing OAuth callback");
			oauthCallback(callback);
		}
		if (isIntentPending) {
			isIntentPending = false;
			processIntent();
		}
	}

	private void processIntent()
	{
		Intent intent = getIntent();
		if (intent == null || isOAuthIntent(intent))
			return;
		UsbDevice device = getUsbDevice(intent);
		if (device == null)
			return;
		try {
			setUsbDevice(device);
		} catch (LinkageError e) {
			Log.w(TAG + " processIntent", "native not ready, scheduling retry", e);
			isIntentPending = true;
			new Handler(Looper.getMainLooper()).postDelayed(() -> {
				if (!isIntentPending)
					return;
				isIntentPending = false;
				UsbDevice retryDevice = getUsbDevice(getIntent());
				if (retryDevice == null)
					return;
				try {
					setUsbDevice(retryDevice);
				} catch (LinkageError retryError) {
					Log.e(TAG + " processIntent", "native still not ready after retry, giving up", retryError);
				}
			}, 500);
		}
	}

	private final BroadcastReceiver usbReceiver = new BroadcastReceiver()
	{
		public void onReceive(Context context, Intent intent) {
			String action = intent.getAction();
			if ("org.subsurfacedivelog.mobile.USB_PERMISSION".equals(action)) {
				synchronized (this) {
					if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
						UsbDevice device = getUsbDevice(intent);
						if (device == null)
							return;
						restartDownload(device);
					} else {
						Log.d(TAG, "USB device permission denied");
					}
				}
			}
		}
	};

	public static Context getAppContext()
	{
		return appContext;
	}

	public static Activity getCurrentActivity()
	{
		return currentActivity;
	}

	@SuppressWarnings("deprecation")
	private static UsbDevice getUsbDevice(Intent intent) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
			return intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class);
		return intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
	}
}
