/**
 * Aimux Professional Dashboard - Advanced Real-time WebSocket Streaming
 * Task 2.3 Implementation
 *
 * This dashboard provides comprehensive real-time monitoring with:
 * - WebSocket streaming with automatic reconnection
 * - Professional Chart.js visualizations
 * - Advanced metrics aggregation and display
 * - Performance monitoring and analytics
 * - Historical data trending
 * - Responsive design with toast notifications
 */

class AimuxDashboard {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;
        this.reconnectDelay = 1000;
        this.isStreaming = false;
        this.updateFrequency = 2000;
        this.charts = {};
        this.metrics = {
            providers: new Map(),
            system: {},
            historical: {},
            performance: {}
        };
        this.streamingStartTime = null;

        // Configuration
        this.config = {
            maxDataPoints: 60,
            chartColors: {
                primary: '#2563eb',
                success: '#16a34a',
                warning: '#f59e0b',
                error: '#dc2626',
                info: '#0891b2'
            },
            providerColors: [
                '#2563eb', '#dc2626', '#16a34a', '#f59e0b',
                '#8b5cf6', '#ec4899', '#06b6d4', '#f97316'
            ]
        };

        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.showLoadingOverlay();
        await this.loadInitialData();
        this.initializeCharts();
        this.hideLoadingOverlay();
        this.updateCurrentTime();
        this.showToast('Dashboard initialized successfully', 'success');
    }

    setupEventListeners() {
        // Streaming controls
        document.getElementById('toggle-streaming').addEventListener('click', () => this.toggleStreaming());
        document.getElementById('request-update').addEventListener('click', () => this.requestManualUpdate());
        document.getElementById('clear-charts').addEventListener('click', () => this.clearCharts());

        // Configuration controls
        document.getElementById('update-frequency').addEventListener('change', (e) => {
            this.updateFrequency = parseInt(e.target.value);
            if (this.isStreaming) {
                this.stopStreaming();
                this.startStreaming();
            }
        });

        document.getElementById('show-detailed').addEventListener('change', (e) => {
            this.toggleDetailedMetrics(e.target.checked);
        });

        document.getElementById('enable-smoothing').addEventListener('change', (e) => {
            this.toggleChartSmoothing(e.target.checked);
        });

        // Provider table refresh
        document.getElementById('refresh-providers').addEventListener('click', () => {
            this.updateProvidersTable();
        });

        // Chart zoom controls
        document.querySelectorAll('.chart-zoom').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const chartName = e.target.dataset.chart;
                this.resetChartZoom(chartName);
            });
        });

        // WebSocket connection management
        window.addEventListener('online', () => {
            this.showToast('Connection restored', 'success');
            if (this.isStreaming) {
                this.connectWebSocket();
            }
        });

        window.addEventListener('offline', () => {
            this.showToast('Connection lost', 'error');
            this.updateConnectionStatus(false);
        });
    }

    async loadInitialData() {
        try {
            // Load comprehensive metrics
            const comprehensiveResponse = await fetch('/metrics/comprehensive');
            if (comprehensiveResponse.ok) {
                const data = await comprehensiveResponse.json();
                this.processMetricsData(data);
            }

            // Load performance stats
            const performanceResponse = await fetch('/metrics/performance');
            if (performanceResponse.ok) {
                const perfData = await performanceResponse.json();
                this.metrics.performance = perfData;
                this.updatePerformanceMetrics();
            }

            // Load historical data
            const historyResponse = await fetch('/metrics/history');
            if (historyResponse.ok) {
                const historyData = await historyResponse.json();
                this.metrics.historical = historyData;
                this.updateHistoricalCharts();
            }
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.showToast('Failed to load initial data', 'error');
        }
    }

    initializeCharts() {
        const chartOptions = {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: {
                legend: {
                    position: 'top',
                    labels: {
                        usePointStyle: true,
                        padding: 15
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#ffffff',
                    bodyColor: '#ffffff',
                    borderColor: '#2563eb',
                    borderWidth: 1,
                    padding: 12,
                    displayColors: true,
                    callbacks: {
                        label: (context) => {
                            let label = context.dataset.label || '';
                            if (label) {
                                label += ': ';
                            }
                            if (context.parsed.y !== null) {
                                label += context.parsed.y.toFixed(2);
                            }
                            return label;
                        }
                    }
                },
                zoom: {
                    zoom: {
                        wheel: {
                            enabled: true,
                        },
                        pinch: {
                            enabled: true
                        },
                        mode: 'x',
                    },
                    pan: {
                        enabled: true,
                        mode: 'x',
                    }
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time'
                    },
                    grid: {
                        color: 'rgba(0, 0, 0, 0.05)'
                    }
                },
                y: {
                    display: true,
                    grid: {
                        color: 'rgba(0, 0, 0, 0.05)'
                    }
                }
            }
        };

        // Response Times Chart
        const responseTimesCtx = document.getElementById('response-times-chart').getContext('2d');
        this.charts.responseTimes = new Chart(responseTimesCtx, {
            type: 'line',
            data: {
                datasets: []
            },
            options: {
                ...chartOptions,
                scales: {
                    ...chartOptions.scales,
                    y: {
                        ...chartOptions.scales.y,
                        title: {
                            display: true,
                            text: 'Response Time (ms)'
                        }
                    }
                }
            }
        });

        // Success Rates Chart
        const successRatesCtx = document.getElementById('success-rates-chart').getContext('2d');
        this.charts.successRates = new Chart(successRatesCtx, {
            type: 'line',
            data: {
                datasets: []
            },
            options: {
                ...chartOptions,
                scales: {
                    ...chartOptions.scales,
                    y: {
                        ...chartOptions.scales.y,
                        min: 0,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Success Rate (%)'
                        }
                    }
                }
            }
        });

        // Request Volume Chart
        const requestVolumeCtx = document.getElementById('request-volume-chart').getContext('2d');
        this.charts.requestVolume = new Chart(requestVolumeCtx, {
            type: 'bar',
            data: {
                datasets: []
            },
            options: {
                ...chartOptions,
                scales: {
                    ...chartOptions.scales,
                    y: {
                        ...chartOptions.scales.y,
                        min: 0,
                        title: {
                            display: true,
                            text: 'Requests per Minute'
                        }
                    }
                }
            }
        });

        // System Resources Chart
        const systemResourcesCtx = document.getElementById('system-resources-chart').getContext('2d');
        this.charts.systemResources = new Chart(systemResourcesCtx, {
            type: 'line',
            data: {
                datasets: [
                    {
                        label: 'CPU Usage (%)',
                        data: [],
                        borderColor: this.config.chartColors.primary,
                        backgroundColor: 'rgba(37, 99, 235, 0.1)',
                        tension: 0.4,
                        fill: true
                    },
                    {
                        label: 'Memory Usage (%)',
                        data: [],
                        borderColor: this.config.chartColors.warning,
                        backgroundColor: 'rgba(245, 158, 11, 0.1)',
                        tension: 0.4,
                        fill: true
                    }
                ]
            },
            options: {
                ...chartOptions,
                scales: {
                    ...chartOptions.scales,
                    y: {
                        ...chartOptions.scales.y,
                        min: 0,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Usage (%)'
                        }
                    }
                }
            }
        });

        // Initialize historical charts
        this.initializeHistoricalCharts();
    }

    initializeHistoricalCharts() {
        const historicalOptions = {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#ffffff',
                    bodyColor: '#ffffff',
                    borderColor: '#2563eb',
                    borderWidth: 1
                }
            },
            scales: {
                x: {
                    display: false
                },
                y: {
                    display: true,
                    grid: {
                        color: 'rgba(0, 0, 0, 0.05)'
                    }
                }
            }
        };

        // Historical Response Times
        const histResponseTimesCtx = document.getElementById('historical-response-times').getContext('2d');
        this.charts.historicalResponseTimes = new Chart(histResponseTimesCtx, {
            type: 'line',
            data: {
                labels: Array.from({length: 60}, (_, i) => i),
                datasets: [{
                    data: [],
                    borderColor: this.config.chartColors.primary,
                    backgroundColor: 'rgba(37, 99, 235, 0.1)',
                    tension: 0.4,
                    fill: true,
                    pointRadius: 1,
                    pointHoverRadius: 3
                }]
            },
            options: {
                ...historicalOptions,
                scales: {
                    ...historicalOptions.scales,
                    y: {
                        ...historicalOptions.scales.y,
                        title: {
                            display: true,
                            text: 'Response Time (ms)'
                        }
                    }
                }
            }
        });

        // Historical Success Rates
        const histSuccessRatesCtx = document.getElementById('historical-success-rates').getContext('2d');
        this.charts.historicalSuccessRates = new Chart(histSuccessRatesCtx, {
            type: 'line',
            data: {
                labels: Array.from({length: 60}, (_, i) => i),
                datasets: [{
                    data: [],
                    borderColor: this.config.chartColors.success,
                    backgroundColor: 'rgba(22, 163, 74, 0.1)',
                    tension: 0.4,
                    fill: true,
                    pointRadius: 1,
                    pointHoverRadius: 3
                }]
            },
            options: {
                ...historicalOptions,
                scales: {
                    ...historicalOptions.scales,
                    y: {
                        ...historicalOptions.scales.y,
                        min: 0,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Success Rate (%)'
                        }
                    }
                }
            }
        });

        // Historical Volume
        const histVolumeCtx = document.getElementById('historical-volume').getContext('2d');
        this.charts.historicalVolume = new Chart(histVolumeCtx, {
            type: 'bar',
            data: {
                labels: Array.from({length: 60}, (_, i) => i),
                datasets: [{
                    data: [],
                    backgroundColor: this.config.chartColors.info,
                    borderColor: this.config.chartColors.info,
                    borderWidth: 1,
                    barThickness: 3
                }]
            },
            options: {
                ...historicalOptions,
                scales: {
                    ...historicalOptions.scales,
                    y: {
                        ...historicalOptions.scales.y,
                        min: 0,
                        title: {
                            display: true,
                            text: 'Requests/Min'
                        }
                    }
                }
            }
        });

        // Historical Resources
        const histResourcesCtx = document.getElementById('historical-resources').getContext('2d');
        this.charts.historicalResources = new Chart(histResourcesCtx, {
            type: 'line',
            data: {
                labels: Array.from({length: 60}, (_, i) => i),
                datasets: [
                    {
                        label: 'CPU',
                        data: [],
                        borderColor: this.config.chartColors.primary,
                        backgroundColor: 'rgba(37, 99, 235, 0.1)',
                        tension: 0.4,
                        fill: true,
                        pointRadius: 1
                    },
                    {
                        label: 'Memory',
                        data: [],
                        borderColor: this.config.chartColors.warning,
                        backgroundColor: 'rgba(245, 158, 11, 0.1)',
                        tension: 0.4,
                        fill: true,
                        pointRadius: 1
                    }
                ]
            },
            options: {
                ...historicalOptions,
                plugins: {
                    ...historicalOptions.plugins,
                    legend: {
                        display: true,
                        position: 'top'
                    }
                },
                scales: {
                    ...historicalOptions.scales,
                    y: {
                        ...historicalOptions.scales.y,
                        min: 0,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Usage (%)'
                        }
                    }
                }
            }
        });
    }

    toggleStreaming() {
        if (this.isStreaming) {
            this.stopStreaming();
        } else {
            this.startStreaming();
        }
    }

    async startStreaming() {
        try {
            await this.connectWebSocket();
            this.isStreaming = true;
            this.streamingStartTime = Date.now();
            document.getElementById('toggle-streaming').textContent = 'Stop Streaming';
            document.getElementById('toggle-streaming').classList.add('btn-warning');
            document.getElementById('toggle-streaming').classList.remove('btn-primary');
            this.showToast('Real-time streaming started', 'success');
        } catch (error) {
            console.error('Failed to start streaming:', error);
            this.showToast('Failed to start streaming', 'error');
        }
    }

    stopStreaming() {
        if (this.ws) {
            this.ws.close();
        }
        this.isStreaming = false;
        document.getElementById('toggle-streaming').textContent = 'Start Streaming';
        document.getElementById('toggle-streaming').classList.add('btn-primary');
        document.getElementById('toggle-streaming').classList.remove('btn-warning');
        this.updateConnectionStatus(false);
        this.showToast('Real-time streaming stopped', 'info');
    }

    async connectWebSocket() {
        return new Promise((resolve, reject) => {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;

            this.ws = new WebSocket(wsUrl);

            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.reconnectAttempts = 0;
                this.updateConnectionStatus(true);

                // Send initial message to get current data
                this.ws.send(JSON.stringify({type: 'request_update'}));

                resolve();
            };

            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.processWebSocketMessage(data);
                } catch (error) {
                    console.error('Failed to parse WebSocket message:', error);
                }
            };

            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.updateConnectionStatus(false);

                if (this.isStreaming && this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.reconnectAttempts++;
                    const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1);

                    console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts}) in ${delay}ms`);

                    setTimeout(() => {
                        this.connectWebSocket().catch(error => {
                            console.error('Reconnection failed:', error);
                            if (this.reconnectAttempts >= this.maxReconnectAttempts) {
                                this.showToast('Failed to reconnect after maximum attempts', 'error');
                                this.stopStreaming();
                            }
                        });
                    }, delay);
                }
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                reject(error);
            };

            // Connection timeout
            setTimeout(() => {
                if (this.ws.readyState === WebSocket.CONNECTING) {
                    this.ws.close();
                    reject(new Error('WebSocket connection timeout'));
                }
            }, 5000);
        });
    }

    processWebSocketMessage(data) {
        if (data.type === 'error') {
            this.showToast(data.message, 'error');
            return;
        }

        this.processMetricsData(data);
        this.updateLastUpdateTime();
    }

    processMetricsData(data) {
        // Store historical data for charts
        const timestamp = new Date().toLocaleTimeString();

        // Process provider metrics
        if (data.providers) {
            Object.entries(data.providers).forEach(([name, metrics]) => {
                this.metrics.providers.set(name, {
                    ...this.metrics.providers.get(name),
                    ...metrics,
                    lastUpdate: timestamp
                });
            });
        }

        // Process system metrics
        if (data.system) {
            this.metrics.system = {
                ...this.metrics.system,
                ...data.system,
                lastUpdate: timestamp
            };
        }

        // Process historical data
        if (data.historical) {
            this.metrics.historical = data.historical;
        }

        // Process performance metrics
        if (data.performance) {
            this.metrics.performance = {
                ...this.metrics.performance,
                ...data.performance
            };
        }

        // Update all UI components
        this.updateMetricsOverview();
        this.updateCharts();
        this.updateProvidersTable();
        this.updatePerformanceMetrics();
        this.updateHistoricalCharts();
    }

    updateMetricsOverview() {
        // System status
        const systemStatus = this.calculateOverallSystemStatus();
        document.getElementById('system-status').textContent = systemStatus.text;
        document.getElementById('system-status').style.color = systemStatus.color;

        // Active providers
        const activeProviders = Array.from(this.metrics.providers.values())
            .filter(p => p.status === 'healthy' || p.status === 'degraded').length;
        document.getElementById('active-providers').textContent = activeProviders;
        document.getElementById('total-providers').textContent = this.metrics.providers.size;

        // Requests per second
        const requestsPerSec = this.metrics.system.requests_per_second || 0;
        document.getElementById('requests-per-sec').textContent = requestsPerSec.toFixed(1);

        // WebSocket connections
        const wsConnections = this.metrics.performance.current_connections || 0;
        const peakConnections = this.metrics.performance.peak_connections || 0;
        document.getElementById('ws-connections').textContent = wsConnections;
        document.getElementById('peak-connections').textContent = peakConnections;
    }

    updateCharts() {
        const timestamp = new Date().toLocaleTimeString();

        // Update response times chart
        this.updateLineChart(this.charts.responseTimes, this.metrics.providers,
                           (provider, name) => ({
                               label: `${name} Response Time`,
                               data: [{x: timestamp, y: provider.avg_response_time_ms || 0}],
                               borderColor: this.getProviderColor(name),
                               backgroundColor: this.getProviderColor(name, 0.1),
                               tension: 0.4,
                               pointRadius: 3
                           }));

        // Update success rates chart
        this.updateLineChart(this.charts.successRates, this.metrics.providers,
                           (provider, name) => ({
                               label: `${name} Success Rate`,
                               data: [{x: timestamp, y: provider.success_rate || 0}],
                               borderColor: this.getProviderColor(name),
                               backgroundColor: this.getProviderColor(name, 0.1),
                               tension: 0.4,
                               pointRadius: 3
                           }));

        // Update request volume chart
        this.updateLineChart(this.charts.requestVolume, this.metrics.providers,
                           (provider, name) => ({
                               label: `${name} Requests/min`,
                               data: [{x: timestamp, y: provider.requests_last_min || 0}],
                               backgroundColor: this.getProviderColor(name, 0.8),
                               borderColor: this.getProviderColor(name),
                               borderWidth: 1,
                               barThickness: 20
                           }));

        // Update system resources chart
        this.updateSystemResourcesChart(timestamp);
    }

    updateLineChart(chart, providers, dataTransform) {
        const datasets = Array.from(providers.entries()).map(([name, provider]) =>
            dataTransform(provider, name)
        );

        datasets.forEach(dataset => {
            const existingDataset = chart.data.datasets.find(d => d.label === dataset.label);
            if (existingDataset) {
                existingDataset.data.push(dataset.data[0]);
                if (existingDataset.data.length > this.config.maxDataPoints) {
                    existingDataset.data.shift();
                }
            } else {
                chart.data.datasets.push(dataset);
            }
        });

        // Update labels
        if (chart.data.labels.length === 0 ||
            chart.data.labels[chart.data.labels.length - 1] !== datasets[0]?.data[0]?.x) {
            if (datasets[0]?.data[0]?.x) {
                chart.data.labels.push(datasets[0].data[0].x);
                if (chart.data.labels.length > this.config.maxDataPoints) {
                    chart.data.labels.shift();
                }
            }
        }

        chart.update('none'); // Update without animation for better performance
    }

    updateSystemResourcesChart(timestamp) {
        const systemData = this.charts.systemResources.data.datasets;

        // Update CPU usage
        systemData[0].data.push({
            x: timestamp,
            y: this.metrics.system.cpu?.current || 0
        });

        // Update memory usage
        systemData[1].data.push({
            x: timestamp,
            y: this.metrics.system.memory?.percent || 0
        });

        // Limit data points
        systemData.forEach(dataset => {
            if (dataset.data.length > this.config.maxDataPoints) {
                dataset.data.shift();
            }
        });

        // Update labels
        this.charts.systemResources.data.labels.push(timestamp);
        if (this.charts.systemResources.data.labels.length > this.config.maxDataPoints) {
            this.charts.systemResources.data.labels.shift();
        }

        this.charts.systemResources.update('none');
    }

    updateHistoricalCharts() {
        const historical = this.metrics.historical;
        if (!historical || !historical.response_times) return;

        // Update historical response times
        if (this.charts.historicalResponseTimes) {
            this.charts.historicalResponseTimes.data.datasets[0].data = historical.response_times.slice(-60);
            this.charts.historicalResponseTimes.update('none');
        }

        // Update historical success rates
        if (this.charts.historicalSuccessRates) {
            this.charts.historicalSuccessRates.data.datasets[0].data = historical.success_rates.slice(-60);
            this.charts.historicalSuccessRates.update('none');
        }

        // Update historical volume
        if (this.charts.historicalVolume) {
            this.charts.historicalVolume.data.datasets[0].data = historical.requests_per_min.slice(-60);
            this.charts.historicalVolume.update('none');
        }

        // Update historical resources
        if (this.charts.historicalResources) {
            this.charts.historicalResources.data.datasets[0].data = historical.cpu_usage.slice(-60);
            this.charts.historicalResources.data.datasets[1].data = historical.memory_usage.slice(-60);
            this.charts.historicalResources.update('none');
        }
    }

    updateProvidersTable() {
        const tbody = document.getElementById('providers-tbody');
        tbody.innerHTML = '';

        this.metrics.providers.forEach((provider, name) => {
            const row = document.createElement('tr');

            // Status badge class
            const statusClass = provider.status === 'healthy' ? 'healthy' :
                              provider.status === 'degraded' ? 'degraded' : 'unhealthy';

            row.innerHTML = `
                <td><strong>${name}</strong></td>
                <td><span class="status-badge ${statusClass}">${provider.status}</span></td>
                <td>${(provider.avg_response_time_ms || 0).toFixed(2)}</td>
                <td>${(provider.success_rate || 0).toFixed(1)}%</td>
                <td>${provider.requests_last_min || 0}</td>
                <td>${provider.last_request || 'never'}</td>
                <td>$${(provider.cost_per_hour || 0).toFixed(4)}</td>
                <td>${(provider.tokens_used?.total || 0).toLocaleString()}</td>
                <td>${provider.rate_limited ?
                    '<span class="status-badge rate-limited">Yes</span>' :
                    '<span class="status-badge healthy">No</span>'}</td>
            `;

            tbody.appendChild(row);
        });

        if (this.metrics.providers.size === 0) {
            tbody.innerHTML = '<tr><td colspan="9" style="text-align: center; color: #6b7280;">No providers configured</td></tr>';
        }
    }

    updatePerformanceMetrics() {
        const perf = this.metrics.performance;

        document.getElementById('avg-update-time').textContent =
            `${(perf.avg_update_time_ms || 0).toFixed(2)}ms`;
        document.getElementById('total-updates').textContent =
            (perf.total_updates || 0).toLocaleString();
        document.getElementById('avg-broadcast-time').textContent =
            `${(perf.avg_broadcast_time_ms || 0).toFixed(2)}ms`;
        document.getElementById('total-broadcasts').textContent =
            (perf.total_broadcasts || 0).toLocaleString();
        document.getElementById('messages-sent').textContent =
            (perf.messages_sent || 0).toLocaleString();
        document.getElementById('messages-dropped').textContent =
            (perf.messages_dropped || 0).toLocaleString();
        document.getElementById('failed-connections').textContent =
            (perf.failed_connections || 0).toLocaleString();

        // Update streaming uptime
        if (this.streamingStartTime) {
            const uptime = Math.floor((Date.now() - this.streamingStartTime) / 1000);
            const minutes = Math.floor(uptime / 60);
            const seconds = uptime % 60;
            document.getElementById('streaming-uptime').textContent =
                minutes > 0 ? `${minutes}m ${seconds}s` : `${seconds}s`;
        }
    }

    calculateOverallSystemStatus() {
        const providers = Array.from(this.metrics.providers.values());
        const healthyCount = providers.filter(p => p.status === 'healthy').length;
        const degradedCount = providers.filter(p => p.status === 'degraded').length;
        const unhealthyCount = providers.filter(p => p.status === 'unhealthy').length;

        if (providers.length === 0) {
            return { text: 'No Providers', color: '#6b7280' };
        }

        if (unhealthyCount === 0 && degradedCount === 0) {
            return { text: 'All Systems Operational', color: '#16a34a' };
        }

        if (unhealthyCount > 0) {
            return { text: 'System Degraded', color: '#dc2626' };
        }

        if (degradedCount > 0) {
            return { text: 'Partial Degradation', color: '#f59e0b' };
        }

        return { text: 'Unknown Status', color: '#6b7280' };
    }

    async requestManualUpdate() {
        try {
            const response = await fetch('/metrics/comprehensive');
            if (response.ok) {
                const data = await response.json();
                this.processMetricsData(data);
                this.updateLastUpdateTime();
                this.showToast('Manual update completed', 'success');
            } else {
                throw new Error('Failed to fetch metrics');
            }
        } catch (error) {
            console.error('Manual update failed:', error);
            this.showToast('Manual update failed', 'error');
        }
    }

    clearCharts() {
        Object.values(this.charts).forEach(chart => {
            if (chart && chart.data) {
                chart.data.labels = [];
                chart.data.datasets.forEach(dataset => {
                    dataset.data = [];
                });
                chart.update();
            }
        });

        this.showToast('Charts cleared', 'info');
    }

    resetChartZoom(chartName) {
        const chart = this.charts[chartName];
        if (chart && chart.resetZoom) {
            chart.resetZoom();
            this.showToast(`Zoom reset for ${chartName.replace(/([A-Z])/g, ' $1').toLowerCase()}`, 'info');
        }
    }

    toggleDetailedMetrics(show) {
        // Implementation for showing/hiding detailed metrics
        document.querySelectorAll('.metric-detailed').forEach(el => {
            el.style.display = show ? 'block' : 'none';
        });
    }

    toggleChartSmoothing(enable) {
        Object.values(this.charts).forEach(chart => {
            if (chart && chart.data) {
                chart.data.datasets.forEach(dataset => {
                    dataset.tension = enable ? 0.4 : 0;
                });
                chart.update();
            }
        });
    }

    getProviderColor(name, alpha = 1) {
        let hash = 0;
        for (let i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash);
        }

        const index = Math.abs(hash) % this.config.providerColors.length;
        const color = this.config.providerColors[index];

        if (alpha < 1) {
            // Convert hex to rgba
            const r = parseInt(color.slice(1, 3), 16);
            const g = parseInt(color.slice(3, 5), 16);
            const b = parseInt(color.slice(5, 7), 16);
            return `rgba(${r}, ${g}, ${b}, ${alpha})`;
        }

        return color;
    }

    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connection-status');
        const statusDot = statusElement.querySelector('.status-dot');
        const statusText = statusElement.querySelector('.status-text');

        if (connected) {
            statusDot.classList.add('connected');
            statusText.textContent = 'WebSocket: Connected';
        } else {
            statusDot.classList.remove('connected');
            statusText.textContent = 'WebSocket: Disconnected';
        }
    }

    updateLastUpdateTime() {
        const lastUpdateElement = document.getElementById('last-update');
        lastUpdateElement.querySelector('.update-time').textContent = new Date().toLocaleTimeString();
    }

    updateCurrentTime() {
        const timeElement = document.getElementById('current-time');
        timeElement.textContent = new Date().toLocaleString();

        // Update every second
        setInterval(() => {
            timeElement.textContent = new Date().toLocaleString();
        }, 1000);
    }

    showLoadingOverlay() {
        document.getElementById('loading-overlay').classList.remove('hidden');
    }

    hideLoadingOverlay() {
        document.getElementById('loading-overlay').classList.add('hidden');
    }

    showToast(message, type = 'info') {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.textContent = message;

        container.appendChild(toast);

        // Auto-remove after 5 seconds
        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100%)';
            setTimeout(() => {
                container.removeChild(toast);
            }, 300);
        }, 5000);
    }
}

// Initialize dashboard when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.aimuxDashboard = new AimuxDashboard();
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.aimuxDashboard) {
        window.aimuxDashboard.stopStreaming();
    }
});