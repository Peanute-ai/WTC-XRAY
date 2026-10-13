#pragma once

// Guard RAII para scatter handle de VMMDLL. Cierra el handle en cualquier ruta
// de salida (return temprano, excepción, etc.). Evita fugas de handles (~60/s)
// que degradan la FPGA.
struct scatter_handle_guard_t
{
	VMMDLL_SCATTER_HANDLE handle = nullptr;

	explicit scatter_handle_guard_t(VMMDLL_SCATTER_HANDLE h) noexcept : handle(h) {}
	~scatter_handle_guard_t() noexcept
	{
		if (handle)
			TargetProcess->CloseScatterHandle(handle);
	}

	scatter_handle_guard_t(const scatter_handle_guard_t&) = delete;
	scatter_handle_guard_t& operator=(const scatter_handle_guard_t&) = delete;

	scatter_handle_guard_t(scatter_handle_guard_t&& other) noexcept
		: handle(other.handle) {
		other.handle = nullptr;
	}

	scatter_handle_guard_t& operator=(scatter_handle_guard_t&& other) noexcept
	{
		if (this != &other)
		{
			if (handle) TargetProcess->CloseScatterHandle(handle);
			handle = other.handle;
			other.handle = nullptr;
		}
		return *this;
	}

	explicit operator bool() const noexcept { return handle != nullptr; }
};

class c_camera {
public:
	auto init(uintptr_t cGame) -> bool
	{
		uintptr_t addr = cGame + offsets::cgame_offsets::camera_offset;
		this->base_address = TargetProcess->Read<uintptr_t>(addr);
		return (this->base_address != 0);
	}

public:

	auto getPosition() -> vec3_t
	{
		return TargetProcess->Read<vec3_t>(
			this->base_address
			+ offsets::cgame_offsets::camera_offsets::camera_position_offset
		);
	}

	auto getCameraMatrix() -> ViewMatrix_t
	{
		ViewMatrix_t matrix{};

		scatter_handle_guard_t guard(TargetProcess->CreateScatterHandle());
		if (!guard)
			return matrix;

		if (!TargetProcess->AddScatterReadRequest(
			guard.handle,
			uintptr_t(this->base_address + offsets::cgame_offsets::camera_offsets::camera_matrix_offset),
			&matrix,
			sizeof(ViewMatrix_t)) ||
			!TargetProcess->ExecuteReadScatter(guard.handle, 0, true))
		{
			return ViewMatrix_t{};
		}

		return matrix;
		// guard.~scatter_handle_guard_t() cierra el handle automáticamente.
	}

private:
	uintptr_t base_address;
};