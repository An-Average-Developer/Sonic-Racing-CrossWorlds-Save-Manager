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

        [DllImport("kernel32.dll")]
        private static extern IntPtr VirtualQueryEx(IntPtr hProcess, IntPtr lpAddress, out MEMORY_BASIC_INFORMATION lpBuffer, uint dwLength);

        [StructLayout(LayoutKind.Sequential)]
        private struct MEMORY_BASIC_INFORMATION
        {
            public IntPtr BaseAddress;
            public IntPtr AllocationBase;
            public uint AllocationProtect;
            public IntPtr RegionSize;
            public uint State;
            public uint Protect;
            public uint Type;
        }

        private const int PROCESS_ALL_ACCESS = 0x1F0FFF;
        private const uint PAGE_EXECUTE_READWRITE = 0x40;
        private const uint MEM_COMMIT = 0x1000;
        private const uint MEM_PRIVATE = 0x20000;
        private const uint PAGE_NOACCESS = 0x01;
        private const uint PAGE_READWRITE = 0x04;
        private const uint PAGE_WRITECOPY = 0x08;
        private const uint PAGE_EXECUTE_WRITECOPY = 0x80;
        private const uint PAGE_GUARD = 0x100;
        private const int ScanChunkSize = 16 * 1024 * 1024;

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

        public int ReadInt32(long address)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            byte[] buffer = new byte[4];
            return ReadProcessMemory(_processHandle, new IntPtr(address), buffer, buffer.Length, out int read) && read == 4
                ? BitConverter.ToInt32(buffer, 0)
                : 0;
        }

        public bool WriteInt32(long address, int value)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            byte[] buffer = BitConverter.GetBytes(value);
            return WriteProcessMemory(_processHandle, new IntPtr(address), buffer, buffer.Length, out _);
        }

        // Cheat Engine style "first scan": every 4-byte aligned int32 equal to value in the
        // game's private read/write memory (the heap, where the tickets live).
        public List<long> ScanForInt32(int value)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            var matches = new List<long>();
            byte[] buffer = new byte[ScanChunkSize];
            IntPtr address = IntPtr.Zero;
            long lastAddress = 0;

            while (VirtualQueryEx(_processHandle, address, out MEMORY_BASIC_INFORMATION mbi, (uint)Marshal.SizeOf<MEMORY_BASIC_INFORMATION>()) != IntPtr.Zero)
            {
                long regionBase = mbi.BaseAddress.ToInt64();
                long regionSize = mbi.RegionSize.ToInt64();
                if (regionSize <= 0)
                    break;

                bool writable = mbi.State == MEM_COMMIT &&
                                mbi.Type == MEM_PRIVATE &&
                                (mbi.Protect & PAGE_GUARD) == 0 &&
                                (mbi.Protect & PAGE_NOACCESS) == 0 &&
                                (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;

                if (writable)
                {
                    for (long offset = 0; offset < regionSize; offset += ScanChunkSize)
                    {
                        int toRead = (int)Math.Min(ScanChunkSize, regionSize - offset);
                        if (!ReadProcessMemory(_processHandle, new IntPtr(regionBase + offset), buffer, toRead, out int bytesRead))
                            continue;

                        for (int i = 0; i + 4 <= bytesRead; i += 4)
                        {
                            if (BitConverter.ToInt32(buffer, i) == value)
                                matches.Add(regionBase + offset + i);
                        }
                    }
                }

                long next = regionBase + regionSize;
                if (next <= lastAddress)
                    break;
                lastAddress = next;
                address = new IntPtr(next);
            }

            return matches;
        }

        // Cheat Engine style "next scan": keeps only the candidates that now hold value.
        public List<long> FilterInt32(IEnumerable<long> candidates, int value)
        {
            if (!_isAttached || _processHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Not attached to process");
            }

            var matches = new List<long>();
            byte[] buffer = new byte[4];
            foreach (long candidate in candidates)
            {
                if (ReadProcessMemory(_processHandle, new IntPtr(candidate), buffer, 4, out int read) &&
                    read == 4 && BitConverter.ToInt32(buffer, 0) == value)
                {
                    matches.Add(candidate);
                }
            }

            return matches;
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
