// main.js - JavaScript for wM-Bus Gateway Web Interface

// Function to update dashboard statistics
async function updateDashboardStats() {
    try {
        const response = await fetch('/api/stats');
        if (response.ok) {
            const stats = await response.json();
            
            document.getElementById('total-packets').textContent = stats.total_packets || 0;
            document.getElementById('whitelisted-devices').textContent = stats.whitelisted_devices || 0;
            document.getElementById('uptime').textContent = formatUptime(stats.uptime || 0);
            
            // Update device list
            updateDeviceList(stats.devices || []);
            
            // Update status indicators
            document.getElementById('wifi-status').textContent = stats.wifi_status || 'Unknown';
            document.getElementById('backend-status').textContent = stats.backend_status || 'Unknown';
            document.getElementById('rf-status').textContent = stats.rf_status || 'Unknown';
        }
    } catch (error) {
        console.error('Error updating dashboard stats:', error);
    }
}

// Function to update device list
function updateDeviceList(devices) {
    const tbody = document.querySelector('#device-table tbody');
    tbody.innerHTML = '';
    
    devices.forEach(device => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${device.address}</td>
            <td>${device.rssi || 'N/A'}</td>
            <td>${device.last_seen || 'N/A'}</td>
            <td>${device.whitelisted ? 'Whitelisted' : 'Not Whitelisted'}</td>
        `;
        tbody.appendChild(row);
    });
}

// Helper function to format uptime
function formatUptime(seconds) {
    const days = Math.floor(seconds / (3600 * 24));
    const hours = Math.floor((seconds % (3600 * 24)) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) {
        return `${days}d ${hours}h ${minutes}m`;
    } else if (hours > 0) {
        return `${hours}h ${minutes}m`;
    } else {
        return `${minutes}m`;
    }
}

// Auto-refresh dashboard every 5 seconds
if (document.querySelector('#device-table')) {
    setInterval(updateDashboardStats, 5000);
    // Initial load
    updateDashboardStats();
}

// Common form submission handler
function setupFormHandler(formId, apiEndpoint, successMessage) {
    const form = document.getElementById(formId);
    if (!form) return;
    
    form.addEventListener('submit', async function(e) {
        e.preventDefault();
        
        const formData = new FormData(form);
        const data = {};
        
        for (let [key, value] of formData.entries()) {
            // Try to parse as number if it looks like one
            if (!isNaN(value) && value.trim() !== '') {
                data[key] = Number(value);
            } else {
                data[key] = value;
            }
        }
        
        try {
            const response = await fetch(apiEndpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            });
            
            if (response.ok) {
                alert(successMessage);
            } else {
                alert('Error saving configuration');
            }
        } catch (error) {
            alert('Error: ' + error.message);
        }
    });
}

// Utility function to show loading state
function showLoading(elementId) {
    const element = document.getElementById(elementId);
    if (element) {
        element.innerHTML = '<p>Loading...</p>';
    }
}

// Utility function to show error
function showError(elementId, message) {
    const element = document.getElementById(elementId);
    if (element) {
        element.innerHTML = `<p class="error">Error: ${message}</p>`;
    }
}

// Utility function to show success
function showSuccess(elementId, message) {
    const element = document.getElementById(elementId);
    if (element) {
        element.innerHTML = `<p class="success">${message}</p>`;
    }
}

// Initialize any page-specific functionality
document.addEventListener('DOMContentLoaded', function() {
    // Add any page-specific initialization here
    console.log('wM-Bus Gateway UI loaded');
});