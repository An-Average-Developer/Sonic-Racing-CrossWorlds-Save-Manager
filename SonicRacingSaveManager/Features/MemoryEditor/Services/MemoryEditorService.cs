using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;

namespace SonicRacingSaveManager.Features.MemoryEditor.Services
{
    public class MemoryEditorService
    {
        [DllImport("kernel32.dll")]
        private static extern IntPtr OpenProcess(int dwDesiredAccess, bool bInheritHandle, int dwProcessId);

        [DllImport("kernel32.dll")]
        private static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out int lpNumberOfBytesRead);

        [DllImport("kernel32.dll")]
        private static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int nSize, out int lpNumberOfBytesWritten);

        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool VirtualProtectEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flNewProtect, out uint lpflOldProtect);

        private const int PROCESS_ALL_ACCESS = 0x1F0FFF;
        private const uint PAGE_EXECUTE_READWRITE = 0x40;

        private const string DefaultProcessName = "SonicRacingCrossWorldsSteam";

        private IntPtr _processHandle = IntPtr.Zero;
        private bool _isAttached;
        private string _processName = DefaultProcessName;
        private IntPtr _moduleBase = IntPtr.Zero;
        private Process? _targetProcess;

        public bool IsAttached => _isAttached;
        public string ProcessName => _processName;

        /// <summary>
        /// Automatic attach using the default process name. If more than one process
        /// shares that name (e.g. a launcher stub alongside the real game process),
        /// the one using the most memory is picked, since the actual running game is
        /// almost always the heavier of the two.
        /// </summary>
        public bool AttachToProcess()
        {
            return AttachToProcessByName(DefaultProcessName);
        }

        /// <summary>Manual attach to a specific process picked from <see cref="GetCandidateProcesses"/>.</summary>
        public bool AttachToProcess(int processId)
        {
            try
            {
                _targetProcess = Process.GetProcessById(processId);

                _processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
                if (_processHandle == IntPtr.Zero)
                    return false;

                _moduleBase = _targetProcess.MainModule?.BaseAddress ?? IntPtr.Zero;
                if (_moduleBase == IntPtr.Zero)
                {
                    CloseHandle(_processHandle);
                    _processHandle = IntPtr.Zero;
                    return false;
                }

                _processName = _targetProcess.ProcessName;
                _isAttached = true;
                return true;
            }
            catch
            {
                _isAttached = false;
                if (_processHandle != IntPtr.Zero)
                {
                    CloseHandle(_processHandle);
                    _processHandle = IntPtr.Zero;
                }
                return false;
            }
        }

        /// <summary>All currently running processes matching the default game process name.</summary>
        public static IEnumerable<Process> GetCandidateProcesses()
        {
            return Process.GetProcessesByName(DefaultProcessName);
        }

        private bool AttachToProcessByName(string processName)
        {
            try
            {
                var processes = Process.GetProcessesByName(processName);
                if (!processes.Any())
                {
                    return false;
                }

                _targetProcess = processes
                    .OrderByDescending(p => TryGetWorkingSet(p))
                    .First();

                _processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, _targetProcess.Id);

                if (_processHandle == IntPtr.Zero)
                {
                    return false;
                }

                _moduleBase = _targetProcess.MainModule?.BaseAddress ?? IntPtr.Zero;

                if (_moduleBase == IntPtr.Zero)
                {
                    CloseHandle(_processHandle);
                    _processHandle = IntPtr.Zero;
                    return false;
                }

                _processName = processName;
                _isAttached = true;
                return true;
            }
            catch (Exception)
            {
                _isAttached = false;
                if (_processHandle != IntPtr.Zero)
                {
                    CloseHandle(_processHandle);
                    _processHandle = IntPtr.Zero;
                }
                return false;
            }
        }

        private static long TryGetWorkingSet(Process process)
        {
            try { return process.WorkingSet64; }
            catch { return 0; }
        }

        public void DetachFromProcess()
        {
            if (_processHandle != IntPtr.Zero)
            {
                CloseHandle(_processHandle);
                _processHandle = IntPtr.Zero;
            }
            _isAttached = false;
            _targetProcess = null;
        }

        public int ReadValue(long baseOffset, int[] offsets)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            try
            {
                IntPtr address = ResolvePointerChain(baseOffset, offsets);

                byte[] buffer = new byte[4];
                if (ReadProcessMemory(_processHandle, address, buffer, buffer.Length, out _))
                {
                    return BitConverter.ToInt32(buffer, 0);
                }

                return 0;
            }
            catch (Exception)
            {
                return 0;
            }
        }

        public bool WriteValue(long baseOffset, int[] offsets, int value)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            try
            {
                IntPtr address = ResolvePointerChain(baseOffset, offsets);

                byte[] buffer = BitConverter.GetBytes(value);
                return WriteProcessMemory(_processHandle, address, buffer, buffer.Length, out _);
            }
            catch (Exception)
            {
                return false;
            }
        }

        public float ReadFloatValue(long baseOffset, int[] offsets)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            try
            {
                IntPtr address = ResolvePointerChain(baseOffset, offsets);

                byte[] buffer = new byte[4];
                if (ReadProcessMemory(_processHandle, address, buffer, buffer.Length, out _))
                {
                    return BitConverter.ToSingle(buffer, 0);
                }

                return 0f;
            }
            catch (Exception)
            {
                return 0f;
            }
        }

        public bool WriteFloatValue(long baseOffset, int[] offsets, float value)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            try
            {
                IntPtr address = ResolvePointerChain(baseOffset, offsets);

                byte[] buffer = BitConverter.GetBytes(value);
                return WriteProcessMemory(_processHandle, address, buffer, buffer.Length, out _);
            }
            catch (Exception)
            {
                return false;
            }
        }

        private IntPtr ResolvePointerChain(long baseOffset, int[] offsets)
        {
            if (offsets == null || offsets.Length == 0)
            {
                return IntPtr.Add(_moduleBase, (int)baseOffset);
            }

            IntPtr address = IntPtr.Add(_moduleBase, (int)baseOffset);

            byte[] buffer = new byte[IntPtr.Size];
            if (!ReadProcessMemory(_processHandle, address, buffer, buffer.Length, out _))
            {
                return IntPtr.Zero;
            }

            address = IntPtr.Size == 8
                ? new IntPtr(BitConverter.ToInt64(buffer, 0))
                : new IntPtr(BitConverter.ToInt32(buffer, 0));

            for (int i = 0; i < offsets.Length - 1; i++)
            {
                address = IntPtr.Add(address, offsets[i]);

                if (!ReadProcessMemory(_processHandle, address, buffer, buffer.Length, out _))
                {
                    return IntPtr.Zero;
                }

                address = IntPtr.Size == 8
                    ? new IntPtr(BitConverter.ToInt64(buffer, 0))
                    : new IntPtr(BitConverter.ToInt32(buffer, 0));
            }

            address = IntPtr.Add(address, offsets[offsets.Length - 1]);

            return address;
        }

        public bool IsProcessRunning()
        {
            var processes = Process.GetProcessesByName(_processName);
            return processes.Any();
        }

        public string GetProcessStatus()
        {
            if (!IsProcessRunning())
            {
                return "Process not running";
            }

            if (_isAttached)
            {
                return "Attached";
            }

            return "Process running (not attached)";
        }

        public bool WriteBytes(long address, byte[] bytes)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            try
            {
                IntPtr targetAddress = new IntPtr(_moduleBase.ToInt64() + address);

                // Change memory protection to writable
                if (!VirtualProtectEx(_processHandle, targetAddress, (uint)bytes.Length, PAGE_EXECUTE_READWRITE, out uint oldProtect))
                    return false;

                // Write the bytes
                bool success = WriteProcessMemory(_processHandle, targetAddress, bytes, bytes.Length, out _);

                // Restore original protection
                VirtualProtectEx(_processHandle, targetAddress, (uint)bytes.Length, oldProtect, out _);

                return success;
            }
            catch (Exception)
            {
                return false;
            }
        }
    }
}
