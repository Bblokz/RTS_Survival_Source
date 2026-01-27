# Unreal Engine C++ Coding Rules (AGENTS.md)

Whenever you work on Unreal Engine C++ code for me, there are rules you **must** follow.  
Ensure to follow the following coding style instructions when writing or refactoring code.

---

## 0) Early Return for `IsValid`

Whenever an `IsValid(Obj)` is used **where all logic lives inside the `if` body**, refactor it into an early return:

- Use `if (not IsValid(Obj))` at the **beginning** of the function.
- If there was an `else` clause, move that code into the new early-return `if` block.

---

## 0.1) Think ahead with booleans
in case it seems very likely that later more conditions will be added where now a boolean will be used
then make it an enum class uint8 with descriptive names instead of a boolean. 
important: do not do this lightly; only if it seems very likely that more conditions will be added later,
so prevent spamming enums everywhere unnecessarily.
Do not do this for a refactor; only when explicitly asked to do so.

## 0.5) Mandatory Validator Functions for Member Pointers

### Context

In controllers or other classes, I often have private pointer members like:

```cpp
UPROPERTY()
UPlayerResourceManager* M_PlayerResourceManager;
```

A design choice for this project is **not to crash**, but instead **log an error** when a pointer is null, using:

```cpp
RTSFunctionLibrary::ReportError
```

### Required Pattern

For member pointers, always declare a validator function like this:

```cpp
bool ACPPController::GetIsValidPlayerResourceManager() const
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_PlayerResourceManager",
		"GetIsValidPlayerResourceManager",
		this
	);

	return false;
}
```

### Rules

- If you see `IsValid(M_SomePointerMember)` **or**
  `M_SomePointerMember.IsValid()` (for weak pointers):
    - Add a validator function like the above.
    - Use that function everywhere instead.
- Remove duplicated error reporting from call sites.
- All error reporting must live **inside the validator function**.
- This is required for both correctness and readability.

---

## 1) Nesting Rules

- Use early returns and extraction to keep nesting to **a maximum of 2 levels**.
- Only exceed this if it is genuinely unavoidable.

---

## 3) Large Loop Bodies

- If a loop body is many lines long, **extract it into a function**.

---

## 4) Function Length Limit

- Break up functions longer than **60 lines**.
- Do **not** do this blindly.
- Only extract logic that is:
    - Logically separate
    - Clearly nameable
- If this is not possible, keep the function as-is.

---

## 5) Function Creation Rules

Whenever you create a new function:

- Use good, descriptive naming
- Keep it reasonably short

---

## 6) Doxygen for Complex Functions (Headers)

In header files, if a function:
- Has multiple parameters **and**
- Is complex enough

Then it **must** have Doxygen comments:

- `@brief`
    - Max **3 lines**
    - Explain **why**, not what
- `@param` for each parameter
- `@return` (if non-void)
    - Max **2 lines**
    - Explain return meaning

**Never** comment things already obvious from names.

---

## 6.5) Private Member Naming (Headers)

- Private members must start with:
    - `M_VarName`
    - `bM_BooleanName` for booleans

---

## 7) Comments for Complex Variables (Headers)

- If a variable is used in complex ways:
    - Add a `//` comment **above it** explaining why
- Never comment variables that are clear from their name

---

## 8) Mandatory Class Doxygen (Headers)

If a class declaration has **no Doxygen comment**, add one:

- `@brief`
    - Max **3 lines**
    - Explain **how the class is used**, not what it is

If the class requires **Blueprint setup**, add:

```cpp
@note FunctionName: call in blueprint to set up etc.
```

- One `@note` per required Blueprint function
- Use a new line for each `@note`
- Never do this for optional Blueprint calls

---

## 9) Switch Statement Rules

- If a `case` contains **more than 3 lines**:
    - Extract it into a function
    - Use proper, descriptive naming

---

## 10) Const Correctness

- Any code you add **must be const-correct**

---

## 11) Unreal Engine Brace Style

- All `{}` must be on their **own line**
- Follow Unreal Engine coding standards

---

## 12) Long `BeginPlay` Functions

`BeginPlay` functions can become very large.  
If you split them into sub-functions that are **only called from `BeginPlay`**, then:

- Name them:  
  `BeginPlay_InitName`

### Example

```cpp
void ASquadUnit::BeginPlay()
{
	Super::BeginPlay();

	// Find the child weapon actor component.
	BeginPlay_SetupChildActorWeaponComp();

	// Set up a timer with lambda to update the speed of the unit on the anim instance periodically.
	BeginPlay_SetupUpdateAnimSpeed();

	// ...
}
```

Where:

```cpp
void ASquadUnit::BeginPlay_SetupUpdateAnimSpeed()
{
	FTimerDelegate TimerDelUpdateAnim;
	TimerDelUpdateAnim.BindLambda([this]()
	{
		if (not IsValid(AnimBp_SquadUnit))
		{
			return;
		}

		AnimBp_SquadUnit->UpdateAnimState(GetVelocity());
	});

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_TimerHandleUpdateAnim,
			TimerDelUpdateAnim,
			DeveloperSettings::Optimisation::UpdateSquadAnimInterval,
			true
		);
	}
}
```

This pattern also applies to other large functions such as `PostInitializeComponents`.  
It should be clear from the header which sub-functions belong to which lifecycle step.

---

## 13) Boolean Negation Style

- Use `not` instead of `!` in `if` statements

---

## 14) Naming Conventions

- All variables must be **CamelCase**
- Follow Unreal Engine naming conventions:
    - `AActor`
    - `FStruct`
- Private members:
    - `M_` prefix
    - `bM_` for booleans

---

## 15) No Magic Numbers

- Avoid magic numbers
- Always use constants
- Declare constants at the **lowest scope possible**
- If shared across classes:
    - Put them in a properly named namespace

---

## 16) Variable Name Quality

- Do **not** use short or sparse names
- Prefer longer, descriptive variable names

---

## 17) Pointer Safety Rules

Always verify pointer safety:

- **UActorComponent derivatives**
    - On owning actor:  
      `UPROPERTY() TObjectPtr<>`
    - On non-owning objects:  
      `UPROPERTY() TWeakObjectPtr<>`
- **Other UObject derivatives**
    - Owned by the class:  
      `UPROPERTY() TObjectPtr<>`
    - Owned externally:  
      `UPROPERTY() TWeakObjectPtr<>`

---

## 18) Delegate & Timer Safety

- Always ensure delegate and timer setups are safe
- Use **WeakObjectPointers** for lambda captures when reasonable

---

## 19) Comment Preservation

- Never remove my comments unless they are:
    - Wrong
    - Misleading due to refactor
- If updated:
    - Fix them to be correct
    - Follow **Rule 8** for header Doxygen comments

---

## 20) Struct Extraction for Related Variables

If **4 or more variables** are related to the same logic inside a class,  
consider grouping them into a struct.

### Example

**Before:**

```cpp
// Flag to indicate if the progress bar is paused
bool bIsPaused;

// Time when the progress bar was paused
float M_PausedTime;

// Accumulated paused duration
float M_TotalPausedDuration;

bool bM_WasHiddenByPause = false;
```

**After:**

```cpp
USTRUCT()
struct FPauseStateTimedProgressBar
{
	GENERATED_BODY()

	// Flag to indicate if the progress bar is paused
	bool bIsPaused;

	// Time when the progress bar was paused
	float M_PausedTime;

	// Accumulated paused duration
	float M_TotalPausedDuration;
~~~~
	bool bM_WasHiddenByPause = false;
}~~~~;
```
## 21) RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object vs RTSFunctionLibrary::ReportErrorVariableNotInitialised
make sure to use the _Object version if the this pointer is not AActor derived.
### Rules

- If the struct is only used by one class:
    - Declare it in the same `.h`
- Otherwise:
    - Put it in its own file
- The struct name must:
    - Be descriptive
    - Clearly indicate it belongs to that class

---
## 22) Always check code for the following issues:

Use this as a **pre-commit / pre-review safety checklist** (especially for Unreal containers and UObject lifetimes). These are the patterns that most often produce **use-after-free**, **out-of-bounds**, or **GC-related dangling pointers**.

### TArray safety & iteration hazards

- **Out-of-bounds array access**  
  Ensure every index is valid at the moment it’s used: `0 <= Index && Index < Array.Num()`. Pay special attention to **stale indices** after removals/compaction and indices cached across frames/ticks.

- **Modifying a `TArray` while iterating with a range-for**  
  `for (auto& Elem : Array)` assumes stable storage. Any `Add/Insert/Remove/Reserve/Shrink` can reallocate or reshuffle, invalidating references/iterators → **use-after-free**. Prefer:
  - index-based loops (often reverse iteration for removals), or
  - staging changes (collect then apply after the loop).

- **Keeping raw pointers/references to `TArray` elements across mutations**  
  Any mutation that can reallocate or move elements (`Add/Insert/Remove/Reserve/Shrink`) can dangle cached `ElementType*` or `ElementType&`. Don’t persist element addresses; persist **indices/keys** (and re-validate) or store stable objects elsewhere.

- **Range-for by reference over a temporary**  
  Avoid `for (auto& X : SomeFunctionReturningTArray())` because references bind into a temporary that dies immediately, leaving `X` dangling. Store the returned array in a named variable (or iterate by value if appropriate).

- **Using `TArray::GetData()` then growing/shrinking**  
  `ElementType* Data = Array.GetData()` is only valid until the array reallocates. Any growth/shrink/move can invalidate it → classic **UAF**. Re-fetch `GetData()` after mutations or avoid caching it across operations.

- **Removing elements with `RemoveAt` / `RemoveAtSwap` but continuing with the old loop index**  
  Removals change indices and/or swap in new elements. If the loop index continues naively, you can skip elements or access invalid indices. Common fixes:
  - iterate backwards for `RemoveAt`, or
  - decrement index after removal, or
  - for `RemoveAtSwap`, re-check the current index because a new element was swapped in.

- **Using `Remove` / `RemoveAll` with predicates that capture references into the same container**  
  During removal/compaction, elements move. If your predicate captures `&Array[i]`, `GetData()`, or element references/pointers, those can become invalid mid-operation. Capture only safe, external data (or copy what you need).

- **Iterating a `TArray` and calling functions that mutate the same array indirectly**  
  Watch for re-entrant mutation via delegates/events/callbacks. If code inside the loop can modify the array (even indirectly), you can invalidate the ongoing iteration and crash. Consider:
  - copying the items to iterate, or
  - guarding against re-entrancy, or
  - moving mutation out of the iteration path.

- **Using `Reserve()` incorrectly then writing past `Num()`**  
  `Reserve()` only increases capacity. Writing to `Array[i]` for `i >= Array.Num()` without `SetNum()` is out-of-bounds. Use `SetNum()`/`AddDefaulted()`/`Emplace()` to grow `Num()` safely.

- **Assuming `IsValidIndex()` stays true after removals**  
  A validity check is only meaningful until the next mutation. If anything can remove/compact between “check” and “use”, the index can become invalid. Re-check right before access or restructure to avoid time gaps.

- **Assuming `TArray` is zero-initialized after `SetNumUninitialized`**  
  Uninitialized memory contains garbage. Reading it (especially as pointers/refs) is undefined behavior and can crash. Use `SetNumZeroed`, `Init`, or explicitly initialize every element before use.

### TMap / TSet mutation, invalidation, and key pitfalls

- **`TMap` pointer/reference invalidation after mutation**  
  Storing `ValueType*`, `ValueType&`, or iterators from `Find()`/iteration and then adding/removing entries can invalidate them due to rehashing/relocation → dangling access. Treat map-held addresses as unstable across mutation.

- **Using `TMap::operator[]` assuming it “just reads”**  
  `Map[Key]` inserts a default value if missing. That mutates the map (allocate/rehash), potentially invalidating previously held pointers/references and altering program state unexpectedly. Prefer `Find`, `FindRef`, or `Contains` when you mean “read-only”.

- **Stale `TMap` keys with pointer types (`UObject*` / `AActor*`)**  
  Raw pointer keys can dangle when objects are destroyed, and later lookups/iteration may touch invalid memory. Prefer weak keys (`TWeakObjectPtr`) or enforce cleanup on destruction.

- **Key mutation after insertion into `TMap`**  
  If the key is a mutable struct and you change fields that affect `GetTypeHash`/`operator==` after insertion, the entry becomes “lost” (can’t be found, iteration/order assumptions break). Treat keys as immutable once inserted.

- **Incorrect custom `GetTypeHash` / `operator==` for map keys**  
  Broken hashing/equality causes missing entries, duplicate “same” keys, or pathological rehash behavior that can appear as corruption-like crashes. Ensure `operator==` is consistent and `GetTypeHash` matches equality semantics.

- **Using `TSet` / `TMap` with UObject keys across GC without weak handling**  
  UObjects can be destroyed while still present in the container; later hashing/equality or iteration may touch invalid object memory. Use weak keys or purge entries on GC/destruction events.

- **Storing `TMap` values as references/pointers to stack data**  
  Inserting pointers to locals (or reference-like wrappers) leaves the map pointing at freed stack memory once the function returns. Store owning values or heap-backed lifetime-managed objects.

- **Iterating `TMap` / `TSet` and calling functions that mutate them indirectly**  
  Re-entrant mutation via callbacks/events can invalidate iterators and in-flight references. If mutation is possible, iterate over a snapshot of keys/values or gate mutation during traversal.

### Threading & lifetime / ownership traps

- **Using `TMap` / `TArray` from the wrong thread**  
  These containers are not generally safe for concurrent mutation. Racing reads/writes—especially with iteration—can corrupt memory and crash nondeterministically. Constrain access to a single thread or use proper synchronization and thread-safe patterns.

- **Capturing `this` (or raw `UObject*`) in a lambda stored/executed later**  
  If the object is destroyed/GC’d before the lambda runs, dereferencing is a crash. Capture `TWeakObjectPtr` and validate at execution time before use.

- **`TArray<UObject*>` without `UPROPERTY()` (or `TObjectPtr`) inside a `UObject`**  
  The GC can’t see raw references, so objects may be collected while the array still holds their pointers → dangling access. Use `UPROPERTY()` with `TObjectPtr<>` (or appropriate GC-visible containers).

- **Storing non-owned memory in a `TArrayView` / `MakeArrayView` past its lifetime**  
  Views do not own data. If the backing buffer/array goes away or reallocates, the view becomes dangling. Keep views strictly within the lifetime of the backing storage.

- **`TMap` / `TArray` holding raw pointers to objects freed elsewhere**  
  Containers don’t manage pointee lifetime. If something else deletes the pointee, you must null/weakify/update entries and handle destruction events to avoid later dereference crashes.

- **Allocator/lifetime mismatch with `TUniquePtr` / `TSharedPtr` stored in containers**  
  Reallocation moves smart pointers correctly, but mixing raw deletes, custom deleters, or storing additional raw aliases elsewhere can produce double-free/UAF when the container is modified. Prefer a single ownership model and avoid raw aliasing unless it’s strictly bounded and validated.

### Low-level memory operations & re-entrancy footguns

- **Using `FMemory::Memcpy` / `Memmove` on non-trivially-copyable element types**  
  For types like `FString`, `TArray`, or smart pointers, bypassing constructors/destructors corrupts internal state and can crash later. Use proper copy/move operations or type-aware utilities.

- **Double-remove / stale handle patterns (cached index/key already removed)**  
  “Remove again” patterns can hit invalid indices or assume `Find` succeeded and dereference null. Always handle “not found” as a normal case, and invalidate cached indices/handles when removals occur.
