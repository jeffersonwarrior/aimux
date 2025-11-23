// Enhanced WebUI Components JavaScript Module
class EnhancedDashboard extends Dashboard {
    constructor() {
        super();
        this.refreshInterval = 5000;
        this.compressionEnabled = false;
        this.realTimeEnabled = true;
        this.alertManager = window.alertManager;
        this.initEnhancedFeatures();
        window.dashboard = this; // Make globally accessible
    }

    initEnhancedFeatures() {
        this.setupEventListeners();
        this.loadSettings();
        this.setupKeyboardShortcuts();
        this.createProviderGrid();
    }

    setupEventListeners() {
        // Listen for provider status changes
        document.addEventListener('provider-status-change', (event) => {
            this.handleProviderStatusChange(event.detail);
        });

        // Listen for system alerts
        document.addEventListener('system-alert', (event) => {
            this.alertManager.show(event.detail.message, event.detail.type, event.detail.duration);
        });

        // Handle window visibility changes
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                this.pauseRealtimeUpdates();
            } else {
                this.resumeRealtimeUpdates();
            }
        });
    }

    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (event) => {
            // Ctrl/Cmd + K for quick provider search
            if ((event.ctrlKey || event.metaKey) && event.key === 'k') {
                event.preventDefault();
                this.showProviderSearch();
            }

            // Escape to close modals
            if (event.key === 'Escape') {
                this.closeAllModals();
            }

            // Ctrl/Cmd + R for manual refresh (override browser refresh)
            if ((event.ctrlKey || event.metaKey) && event.key === 'r') {
                event.preventDefault();
                this.forceRefresh();
            }
        });
    }

    handleRealtimeUpdate(data) {
        super.handleRealtimeUpdate(data);

        // Handle alert notifications
        if (data.type === 'alert') {
            this.alertManager.show(data.message, data.severity, data.duration);
        }

        // Handle provider status changes with alerts
        if (data.type === 'provider-status-change') {
            this.handleProviderStatusChange(data);
        }

        // Handle system-wide alerts
        if (data.type === 'system-alert') {
            showStatusBanner(data.title, data.message, data.type);
        }
    }

    handleProviderStatusChange(data) {
        const { providerId, oldStatus, newStatus, name } = data;

        let alertMessage = `Provider ${name} status changed from ${oldStatus} to ${newStatus}`;
        let alertType = 'info';

        if (newStatus === 'error') {
            alertType = 'error';
            alertMessage = `❌ Provider ${name} is experiencing issues!`;
        } else if (newStatus === 'healthy' && oldStatus !== 'healthy') {
            alertType = 'success';
            alertMessage = `✅ Provider ${name} has recovered!`;
        } else if (newStatus === 'warning') {
            alertType = 'warning';
            alertMessage = `⚠️ Provider ${name} is experiencing high latency or approaching rate limits`;
        }

        this.alertManager.show(alertMessage, alertType, 5000);
    }

    createProviderGrid() {
        // Enhanced provider cards with click handlers
        const container = document.getElementById('provider-cards');
        container.addEventListener('click', (event) => {
            const card = event.target.closest('.provider-card');
            if (card && card.dataset.providerId) {
                showProviderDetails(card.dataset.providerId);
            }
        });
    }

    createProviderCard(provider) {
        const card = super.createProviderCard(provider);
        card.dataset.providerId = provider.id || provider.name;
        card.style.cursor = 'pointer';

        // Add click hint
        const title = card.querySelector('h3');
        title.innerHTML += ' <span class="text-xs text-gray-500">(click for details)</span>';

        // Add quick actions
        const actionsDiv = document.createElement('div');
        actionsDiv.className = 'mt-3 flex space-x-2';
        actionsDiv.innerHTML = `
            <button onclick="event.stopPropagation(); testProvider('${provider.id}')" class="text-xs bg-blue-600 hover:bg-blue-700 px-2 py-1 rounded">Test</button>
            <button onclick="event.stopPropagation(); toggleProvider('${provider.id}')" class="text-xs bg-gray-600 hover:bg-gray-700 px-2 py-1 rounded">${provider.enabled ? 'Disable' : 'Enable'}</button>
        `;
        card.appendChild(actionsDiv);

        return card;
    }

    setRefreshInterval(interval) {
        this.refreshInterval = interval;

        if (interval === 0) {
            this.stopAutoRefresh();
            this.alertManager.show('Auto-refresh disabled', 'info');
        } else {
            this.startAutoRefresh();
            const seconds = interval / 1000;
            this.alertManager.show(`Auto-refresh set to ${seconds} seconds`, 'info');
        }
    }

    setChartType(type) {
        if (this.chart) {
            this.chart.config.type = type === 'area' ? 'line' : type;

            if (type === 'area') {
                this.chart.data.datasets[0].fill = true;
            } else {
                this.chart.data.datasets[0].fill = false;
            }

            this.chart.update();
            this.alertManager.show(`Chart type changed to ${type}`, 'success');
        }
    }

    setTheme(theme) {
        document.body.classList.remove('dark', 'light');

        if (theme === 'auto') {
            const isDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
            document.body.classList.add(isDark ? 'dark' : 'light');
        } else {
            document.body.classList.add(theme);
        }

        localStorage.setItem('aimux-theme', theme);
        this.alertManager.show(`Theme changed to ${theme}`, 'success');
    }

    loadSettings() {
        // Load saved settings from localStorage
        const savedTheme = localStorage.getItem('aimux-theme') || 'dark';
        const savedRefreshInterval = localStorage.getItem('aimux-refresh-interval') || '5000';

        document.getElementById('theme-selector').value = savedTheme;
        document.getElementById('refresh-interval').value = savedRefreshInterval;

        this.setTheme(savedTheme);
        this.setRefreshInterval(parseInt(savedRefreshInterval));
    }

    startAutoRefresh() {
        if (this.refreshInterval > 0) {
            if (this.refreshTimer) {
                clearInterval(this.refreshTimer);
            }

            this.refreshTimer = setInterval(() => {
                if (!document.hidden && this.realTimeEnabled) {
                    this.loadInitialData();
                }
            }, this.refreshInterval);
        }
    }

    stopAutoRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
        }
    }

    pauseRealtimeUpdates() {
        this.realTimeEnabled = false;
        if (this.ws) {
            this.ws.close();
        }
    }

    resumeRealtimeUpdates() {
        this.realTimeEnabled = true;
        if (!this.ws || this.ws.readyState === WebSocket.CLOSED) {
            this.connectWebSocket();
        }
    }

    forceRefresh() {
        this.alertManager.show('Refreshing data...', 'info');
        this.loadInitialData().then(() => {
            this.alertManager.show('Data refreshed successfully!', 'success');
        });
    }

    showProviderSearch() {
        // Implement quick provider search overlay
        const searchHtml = `
            <div id="provider-search-overlay" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
                <div class="bg-gray-800 text-white rounded-lg p-6 max-w-md w-full mx-4">
                    <h3 class="text-lg font-bold mb-4">Search Providers</h3>
                    <input type="text" id="provider-search-input" placeholder="Type provider name..."
                           class="w-full bg-gray-700 rounded px-3 py-2 mb-4"
                           onkeyup="window.dashboard.filterProviders(this.value)">
                    <div id="search-results" class="max-h-60 overflow-y-auto"></div>
                    <button onclick="window.dashboard.closeProviderSearch()"
                            class="mt-4 bg-gray-600 hover:bg-gray-700 px-4 py-2 rounded">Cancel</button>
                </div>
            </div>
        `;

        document.body.insertAdjacentHTML('beforeend', searchHtml);
        document.getElementById('provider-search-input').focus();
    }

    filterProviders(query) {
        const resultsDiv = document.getElementById('search-results');
        const providers = Array.from(document.querySelectorAll('.provider-card'));

        const filtered = providers.filter(card => {
            const name = card.querySelector('h3').textContent.toLowerCase();
            return name.includes(query.toLowerCase());
        });

        resultsDiv.innerHTML = filtered.map(card =>
            `<div class="p-2 hover:bg-gray-700 cursor-pointer" onclick="window.dashboard.selectProvider('${card.dataset.providerId}')">
                ${card.querySelector('h3').textContent}
            </div>`
        ).join('');

        if (filtered.length === 0) {
            resultsDiv.innerHTML = '<div class="p-2 text-gray-400">No providers found</div>';
        }
    }

    selectProvider(providerId) {
        this.closeProviderSearch();
        showProviderDetails(providerId);
    }

    closeProviderSearch() {
        const overlay = document.getElementById('provider-search-overlay');
        if (overlay) {
            overlay.remove();
        }
    }

    closeAllModals() {
        this.closeProviderSearch();
        closeProviderModal();

        const configPanel = document.getElementById('config-panel');
        if (!configPanel.classList.contains('translate-x-full')) {
            toggleConfigPanel();
        }
    }

    // Provider action methods
    async testProvider(providerId) {
        this.alertManager.show(`Testing provider ${providerId}...`, 'info');

        try {
            const response = await fetch(`/api/providers/${providerId}/test`, {
                method: 'POST'
            });
            const result = await response.json();

            if (result.success) {
                this.alertManager.show(`Provider test successful! Response time: ${result.responseTime}ms`, 'success');
            } else {
                this.alertManager.show(`Provider test failed: ${result.error}`, 'error');
            }
        } catch (error) {
            this.alertManager.show(`Provider test failed: ${error.message}`, 'error');
        }
    }

    async toggleProvider(providerId) {
        try {
            const response = await fetch(`/api/providers/${providerId}/toggle`, {
                method: 'POST'
            });
            const result = await response.json();

            if (result.success) {
                this.alertManager.show(`Provider ${result.enabled ? 'enabled' : 'disabled'}`, 'success');
                this.loadInitialData(); // Refresh data
            } else {
                this.alertManager.show(`Failed to toggle provider: ${result.error}`, 'error');
            }
        } catch (error) {
            this.alertManager.show(`Failed to toggle provider: ${error.message}`, 'error');
        }
    }
}

// Provider action functions (for inline onclick handlers)
window.testProvider = function(providerId) {
    if (window.dashboard) {
        window.dashboard.testProvider(providerId);
    }
};

window.toggleProvider = function(providerId) {
    if (window.dashboard) {
        window.dashboard.toggleProvider(providerId);
    }
};

// Initialize enhanced dashboard when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new EnhancedDashboard();
});

// Export utilities for other scripts
window.EnhancedDashboard = EnhancedDashboard;
window.alertManager = new AlertManager();