# Landscape data PCG-node helpers

`PCGMarkLandscapeData` in `PCGMarkLandscapeDataHelpers.h/.cpp` owns behaviour that
is common to every Landscape-marking PCG node:

- channel names used in node titles;
- paint-mode names and deterministic value-only artistic paint configurations;
- a graph-stack CRC that isolates each node invocation's managed resource;
- pass-through of the node's default input to its default output;
- lookup and PCG error reporting for the unique `ALandscapeDataManager`;
- finding or creating `UPCGLandscapeDataManagedResource` instances and initializing
  their stable identity.

Keep point-only work (copying `FPCGPoint` data, point seed variation) in
`PCGMarkPointsInLandscape`. Keep volume-only work (borrowing and synchronously
sampling `UPCGSpatialData`) in `PCGMarkVolumesInLandscape`. Add a helper to this
namespace only when both node types can use it without making their input-specific
logic less clear.
