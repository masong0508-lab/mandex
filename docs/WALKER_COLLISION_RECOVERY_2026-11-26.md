# Walker collision and recovery follow-up

Built on the exact 660-stage Walking Relay Circuit head 22ac14759033cb574ad4ef9e6ec93b41b01ea222. All existing maps, save IDs, three-stage rewards, options and animation changes are retained.

The original moving-debris hitbox is a Cartesian product of three horizontal and five vertical samples. Checking for one valid sample on each axis is logically equivalent to searching all pairs. Strict radius comparisons, dead-cell filtering, chunk broadphase and static wall checks are retained. Browser coordinates are computed once per query rather than repeatedly for each voxel. This reduces redundant collision arithmetic, not collision fidelity; no FPS improvement is asserted without hardware profiling.

Local rubble pop-out now shares the recovery bounds predicate with safe-point and tube exit placement. A wedged walker cannot activate a tube until locally freed or returned to a validated safe point. No new arrays or state fields are added.

Verification executed on the browser production functions: 7,372 regression checks (including a dense sample lattice and ceiling-jam recovery), 30,025 separate old/new predicate comparisons, and full script syntax. Native float/fixed regression coverage was added to the existing walking gate. Fresh CI ROM packaging and on-device performance are not yet verified.
