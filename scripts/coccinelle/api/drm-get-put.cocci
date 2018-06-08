// SPDX-License-Identifier: GPL-2.0
///
/// Use drm_*_get() and drm_*_put() helpers instead of drm_*_reference() and
/// drm_*_unreference() helpers.
///
// Confidence: High
// Copyright: (C) 2017 NVIDIA Corporation
// Options: --no-includes --include-headers
//

virtual patch
virtual report

@depends on patch@
expression object;
@@

(
- drm_gem_object_unreference_unlocked(object)
+ drm_gem_object_put_unlocked(object)
|
- drm_dev_unref(object)
+ drm_dev_put(object)
)

@r depends on report@
expression object;
position p;
@@

(
drm_gem_object_unreference_unlocked(object)
|
drm_dev_unref@p(object)
)

@script:python depends on report@
object << r.object;
p << r.p;
@@

msg="WARNING: use get/put helpers to reference and dereference %s" % (object)
coccilib.report.print_report(p[0], msg)
