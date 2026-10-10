using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace SonicRacingSaveManager.Features.MemoryEditor.Models
{
    public class MemoryValue : INotifyPropertyChanged
    {
        private string _name = string.Empty;
        private int _currentValue;
        private int _newValue;
        private string _description = string.Empty;
        private long _address;

        public string Name
        {
            get => _name;
            set
            {
                _name = value;
                OnPropertyChanged();
            }
        }

        public int CurrentValue
        {
            get => _currentValue;
            set
            {
                _currentValue = value;
                OnPropertyChanged();
            }
        }

        public int NewValue
        {
            get => _newValue;
            set
            {
                _newValue = value;
                OnPropertyChanged();
            }
        }

        public string Description
        {
            get => _description;
            set
            {
                _description = value;
                OnPropertyChanged();
            }
        }

        /// <summary>Absolute address found with the ticket search; 0 until located.</summary>
        public long Address
        {
            get => _address;
            set
            {
                _address = value;
                OnPropertyChanged();
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
