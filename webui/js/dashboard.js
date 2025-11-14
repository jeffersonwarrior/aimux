// Aimux2 Dashboard JavaScript
class Dashboard {
    constructor() {
        this.ws = null;
        this.chart = null;
        this.providers = new Map();
        this.metrics = {
            totalRequests: 0,
            avgResponseTime: 0,
            successRate: 100
        };
        
        this.init();
    }

    init() {
        this.initChart();
        this.connectWebSocket();
        this.loadInitialData();
        this.startAutoRefresh();
    }

    initChart() {
        const ctx = document.getElementById('performance-chart').getContext('2d');
        this.chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Response Time (ms)',
                    data: [],
                    borderColor: 'rgb(59, 130, 246)',
                    backgroundColor: 'rgba(59, 130, 246, 0.1)',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        labels: { color: '#fff' }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#9ca3af' },
                        grid: { color: '#374151' }
                    },
                    y: {
                        ticks: { color: '#9ca3af' },
                        grid: { color: '#374151' }
                    }
                }
            }
        });
    }

    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        try {
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                document.getElementById('connection-status').textContent = 'Connected';
                document.getElementById('connection-status').className = 'text-sm px-2 py-1 rounded bg-green-600';
            };
            
            this.ws.onmessage = (event) => {
                const data = JSON.parse(event.data);
                this.handleRealtimeUpdate(data);
            };
            
            this.ws.onclose = () => {
                document.getElementById('connection-status').textContent = 'Disconnected';
                document.getElementById('connection-status').className = 'text-sm px-2 py-1 rounded bg-red-600';
                // Attempt reconnection after 5 seconds
                setTimeout(() => this.connectWebSocket(), 5000);
            };
            
        } catch (error) {
            console.error('WebSocket connection failed:', error);
            // Fallback to HTTP polling
            this.startHttpPolling();
        }
    }

    async loadInitialData() {
        try {
            const response = await fetch('/api/providers');
            const providers = await response.json();
            this.updateProviderCards(providers);
            
            const metricsResponse = await fetch('/api/metrics');
            const metrics = await metricsResponse.json();
            this.updateMetrics(metrics);
            
        } catch (error) {
            console.error('Failed to load initial data:', error);
        }
    }

    handleRealtimeUpdate(data) {
        if (data.type === 'metrics') {
            this.updateMetrics(data.data);
            this.updateChart(data.data);
        } else if (data.type === 'providers') {
            this.updateProviderCards(data.data);
        }
    }

    updateMetrics(metrics) {
        document.getElementById('total-requests').textContent = metrics.totalRequests || 0;
        document.getElementById('avg-response-time').textContent = `${metrics.avgResponseTime || 0}ms`;
        document.getElementById('success-rate').textContent = `${metrics.successRate || 100}%`;
        document.getElementById('active-providers').textContent = metrics.activeProviders || 0;
        
        this.metrics = { ...this.metrics, ...metrics };
    }

    updateProviderCards(providers) {
        const container = document.getElementById('provider-cards');
        container.innerHTML = '';
        
        providers.forEach(provider => {
            const card = this.createProviderCard(provider);
            container.appendChild(card);
        });
        
        document.getElementById('active-providers').textContent = providers.length;
    }

    createProviderCard(provider) {
        const card = document.createElement('div');
        card.className = `provider-card bg-gray-800 rounded-lg p-4 fade-in ${this.getStatusClass(provider.status)}`;
        
        card.innerHTML = `
            <div class="flex items-center justify-between mb-2">
                <h3 class="text-lg font-semibold">${provider.name}</h3>
                <span class="text-sm px-2 py-1 rounded ${this.getStatusBadgeClass(provider.status)}">${provider.status}</span>
            </div>
            <div class="text-sm text-gray-400 space-y-1">
                <div>Requests: ${provider.requests || 0}</div>
                <div>Avg Time: ${provider.avgResponseTime || 0}ms</div>
                <div>Success Rate: ${provider.successRate || 100}%</div>
                <div>Rate Limit: ${provider.rateLimitRemaining || 'N/A'}</div>
            </div>
        `;
        
        return card;
    }

    getStatusClass(status) {
        switch (status.toLowerCase()) {
            case 'healthy': return 'status-healthy';
            case 'warning': return 'status-warning';
            case 'error': return 'status-error';
            default: return '';
        }
    }

    getStatusBadgeClass(status) {
        switch (status.toLowerCase()) {
            case 'healthy': return 'bg-green-600';
            case 'warning': return 'bg-yellow-600';
            case 'error': return 'bg-red-600';
            default: return 'bg-gray-600';
        }
    }

    updateChart(data) {
        const now = new Date().toLocaleTimeString();
        
        this.chart.data.labels.push(now);
        this.chart.data.datasets[0].data.push(data.avgResponseTime || 0);
        
        // Keep only last 20 data points
        if (this.chart.data.labels.length > 20) {
            this.chart.data.labels.shift();
            this.chart.data.datasets[0].data.shift();
        }
        
        this.chart.update('none');
    }

    startAutoRefresh() {
        setInterval(() => {
            this.loadInitialData();
        }, 5000); // Refresh every 5 seconds
    }

    startHttpPolling() {
        setInterval(async () => {
            try {
                const response = await fetch('/api/metrics');
                const metrics = await response.json();
                this.handleRealtimeUpdate({ type: 'metrics', data: metrics });
            } catch (error) {
                console.error('HTTP polling failed:', error);
            }
        }, 2000);
    }
}

// Initialize dashboard when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new Dashboard());
});
