# Cartoon body volume and valve station support

This update uses one shared body-volume model to keep the silhouette consistent in front, profile, seated and flight art. A bounded, reciprocal axis adjustment preserves ellipsoid volume during cosmetic oscillation. The phase wraps over two cycles so the slower component cannot snap. The side-flight outline, shading and clothing read the same projected dimensions. A fixed open backrest and foot platform are drawn behind the valve station, including at relocated slings. They are scenery, not actor constraints or collision objects. The seated art is scaled closer to the other character poses. Existing release, drag, launch, pressure rules, and performance presets are unchanged.

Tests validate finite geometry, volume conservation, projected dimensions, phase wrapping and station translation. The browser Canvas test checks the support and character silhouette in Ultra Low and High. No new native assets or device benchmarks are claimed.
