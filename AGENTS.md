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
