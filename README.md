## TWRP Device Tree for the Galaxy J5 2016 (j5xnlte)

### How to build
1. Get the 
[minimal-manifest-twrp](https://github.com/minimal-manifest-twrp/platform_manifest_twrp_omni/tree/twrp-8.1)
(omni twrp-8.1 branch) and follow the instructions there

2. Clone this repository to `device/samsung/j5xnlte`
```
git clone https://github.com/prototype74/android_device_samsung_j5xnlte.git -b android-8.1-sdc2 device/samsung/j5xnlte
```

3. Build the recovery image
```bash
export ALLOW_MISSING_DEPENDENCIES=true; . build/envsetup.sh; lunch omni_j5xnlte-eng; mka recoveryimage
```

### Include awk tool (recommended)

Add following content to `.repo/local_manifests/one-true-awk.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
    <project
        name="platform/external/one-true-awk"
        path="external/one-true-awk"
        revision="refs/tags/android-9.0.0_r61"
        remote="aosp" />
</manifest>
```

Then `repo sync` to pull awk
