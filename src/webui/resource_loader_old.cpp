#include "aimux/webui/resource_loader.hpp"
#include <stdexcept>
#include <algorithm>

namespace aimux {
namespace webui {

ResourceLoader& ResourceLoader::getInstance() {
    static ResourceLoader instance;
    return instance;
}

const EmbeddedResource* ResourceLoader::getResource(const std::string& path) const {
    auto it = resources_.find(path);
    if (it != resources_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ResourceLoader::hasResource(const std::string& path) const {
    return resources_.find(path) != resources_.end();
}

std::vector<std::string> ResourceLoader::getResourcePaths() const {
    std::vector<std::string> paths;
    paths.reserve(resources_.size());

    for (const auto& [path, resource] : resources_) {
        paths.push_back(path);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

void ResourceLoader::initialize() {
    // Clear any existing resources
    resources_.clear();

    // Initialize all embedded resources
    initializeDashboardHTML();
    initializeDashboardCSS();
    initializeDashboardJS();
}

std::string ResourceLoader::getContentType(const std::string& extension) const {
    if (extension == ".html") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".ico") return "image/x-icon";

    return "application/octet-stream"; // Default
}

void ResourceLoader::addResource(const std::string& path, const std::string& data, const std::string& content_type) {
    auto resource = std::make_unique<EmbeddedResource>(data, content_type);
    resources_[path] = std::move(resource);
}

void ResourceLoader::initializeDashboardHTML() {
    const std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Aimux Professional Dashboard</title>
    <meta name="description" content="Real-time AI provider monitoring and management dashboard">
    <link rel="stylesheet" href="/dashboard.css">
    <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ccircle cx='50' cy='50' r='45' fill='%232563eb'/%3E%3Ctext x='50' y='65' font-family='Arial' font-size='40' fill='white' text-anchor='middle'%3EA%3C/text%3E%3C/svg%3E">
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.min.js"></script>
</head>
<body>
    <div class="app-container" data-theme="light">
        <!-- Header -->
        <header class="header">
            <div class="header-content">
                <div class="logo">
                    <svg width="32" height="32" viewBox="0 0 32 32" class="logo-icon">
                        <circle cx="16" cy="16" r="14" fill="#2563eb"/>
                        <text x="16" y="22" font-family="Arial" font-size="14" fill="white" text-anchor="middle">A</text>
                    </svg>
                    <h1 class="logo-text">Aimux Dashboard</h1>
                </div>
                <div class="header-controls">
                    <div class="theme-toggle" role="button" tabindex="0" aria-label="Toggle theme">
                        <svg class="theme-icon light-icon" width="20" height="20" viewBox="0 0 20 20" fill="currentColor">
                            <path fill-rule="evenodd" d="M10 2a1 1 0 011 1v1a1 1 0 11-2 0V3a1 1 0 011-1zm4 8a4 4 0 11-8 0 4 4 0 018 0zm-.464 4.95l.707.707a1 1 0 001.414-1.414l-.707-.707a1 1 0 00-1.414 1.414zm2.12-10.607a1 1 0 010 1.414l-.706.707a1 1 0 11-1.414-1.414l.707-.707a1 1 0 011.414 0zM17 11a1 1 0 100-2h-1a1 1 0 100 2h1zm-7 4a1 1 0 011 1v1a1 1 0 11-2 0v-1a1 1 0 011-1zM5.05 6.464A1 1 0 106.465 5.05l-.708-.707a1 1 0 00-1.414 1.414l.707.707zm1.414 8.486l-.707.707a1 1 0 01-1.414-1.414l.707-.707a1 1 0 011.414 1.414zM4 11a1 1 0 100-2H3a1 1 0 000 2h1z" clip-rule="evenodd"/>
                        </svg>
                        <svg class="theme-icon dark-icon" width="20" height="20" viewBox="0 0 20 20" fill="currentColor">
                            <path d="M17.293 13.293A8 8 0 016.707 2.707a8.001 8.001 0 1010.586 10.586z"/>
                        </svg>
                    </div>
                    <div class="connection-status" id="connectionStatus">
                        <span class="status-dot"></span>
                        <span class="status-text">Connected</span>
                    </div>
                </div>
            </div>
        </header>

        <!-- Main Content -->
        <main class="main-content">
            <!-- System Overview Cards -->
            <section class="overview-section">
                <h2 class="section-title">System Overview</h2>
                <div class="overview-grid">
                    <div class="metric-card">
                        <div class="metric-header">
                            <h3>Active Requests</h3>
                            <svg class="metric-icon" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 10V3L4 14h7v7l9-11h-7z"/>
                            </svg>
                        </div>
                        <div class="metric-value" id="activeRequests">0</div>
                        <div class="metric-label">requests/sec</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <h3>Total Requests</h3>
                            <svg class="metric-icon" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M16 8v8m-4-5v5m-4-2v2m-2 4h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z"/>
                            </svg>
                        </div>
                        <div class="metric-value" id="totalRequests">0</div>
                        <div class="metric-label">today</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <h3>Success Rate</h3>
                            <svg class="metric-icon" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/>
                            </svg>
                        </div>
                        <div class="metric-value" id="successRate">0%</div>
                        <div class="metric-label">overall</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <h3>System Uptime</h3>
                            <svg class="metric-icon" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z"/>
                            </svg>
                        </div>
                        <div class="metric-value" id="uptime">0m</div>
                        <div class="metric-label">since start</div>
                    </div>
                </div>
            </section>

            <!-- System Resources -->
            <section class="resources-section">
                <h2 class="section-title">System Resources</h2>
                <div class="resources-grid">
                    <div class="resource-card">
                        <h3>Memory Usage</h3>
                        <div class="resource-chart-container">
                            <canvas id="memoryChart" width="400" height="200"></canvas>
                        </div>
                        <div class="resource-info">
                            <span id="memoryUsage">0 MB</span>
                        </div>
                    </div>

                    <div class="resource-card">
                        <h3>CPU Usage</h3>
                        <div class="resource-chart-container">
                            <canvas id="cpuChart" width="400" height="200"></canvas>
                        </div>
                        <div class="resource-info">
                            <span id="cpuUsage">0%</span>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Provider Status -->
            <section class="providers-section">
                <h2 class="section-title">Provider Status</h2>
                <div class="providers-grid" id="providersGrid">
                    <!-- Provider cards will be dynamically inserted here -->
                </div>
            </section>

            <!-- Request Metrics Chart -->
            <section class="metrics-section">
                <h2 class="section-title">Request Metrics</h2>
                <div class="metrics-grid">
                    <div class="chart-card">
                        <div class="chart-header">
                            <h3>Requests Over Time</h3>
                            <div class="chart-controls">
                                <select id="timeRange" class="time-range-select">
                                    <option value="5m">Last 5 minutes</option>
                                    <option value="15m">Last 15 minutes</option>
                                    <option value="1h" selected>Last hour</option>
                                    <option value="6h">Last 6 hours</option>
                                </select>
                            </div>
                        </div>
                        <div class="chart-container">
                            <canvas id="requestsChart" width="800" height="300"></canvas>
                        </div>
                    </div>

                    <div class="chart-card">
                        <div class="chart-header">
                            <h3>Response Times</h3>
                        </div>
                        <div class="chart-container">
                            <canvas id="responseTimeChart" width="800" height="300"></canvas>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Configuration Section -->
            <section class="config-section">
                <h2 class="section-title">Quick Configuration</h2>
                <div class="config-grid">
                    <div class="config-card">
                        <h3>Provider Settings</h3>
                        <div class="config-content" id="providerConfig">
                            <button class="btn btn-primary" onclick="showProviderModal()">Add Provider</button>
                            <button class="btn btn-secondary" onclick="loadProviderConfig()">Load Config</button>
                        </div>
                    </div>

                    <div class="config-card">
                        <h3>System Settings</h3>
                        <div class="config-content">
                            <div class="setting-item">
                                <label for="refreshInterval">Refresh Interval</label>
                                <select id="refreshInterval">
                                    <option value="1000">1 second</option>
                                    <option value="2000" selected>2 seconds</option>
                                    <option value="5000">5 seconds</option>
                                    <option value="10000">10 seconds</option>
                                </select>
                            </div>
                            <div class="setting-item">
                                <label for="autoRefresh">
                                    <input type="checkbox" id="autoRefresh" checked>
                                    Auto-refresh data
                                </label>
                            </div>
                        </div>
                    </div>
                </div>
            </section>
        </main>

        <!-- Modals -->
        <div id="providerModal" class="modal" role="dialog" aria-labelledby="modalTitle" aria-hidden="true">
            <div class="modal-content">
                <div class="modal-header">
                    <h3 id="modalTitle">Add Provider</h3>
                    <button class="modal-close" onclick="hideProviderModal()" aria-label="Close modal">&times;</button>
                </div>
                <div class="modal-body">
                    <form id="providerForm">
                        <div class="form-group">
                            <label for="providerName">Provider Name</label>
                            <select id="providerName" required>
                                <option value="cerebras">Cerebras</option>
                                <option value="zai">Z.AI</option>
                                <option value="minimax">MiniMax</option>
                                <option value="synthetic">Synthetic (Test)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label for="apiKey">API Key</label>
                            <input type="password" id="apiKey" placeholder="Enter API key" required>
                        </div>
                        <div class="form-group">
                            <label for="endpoint">Endpoint URL</label>
                            <input type="url" id="endpoint" placeholder="https://api.example.com" required>
                        </div>
                        <div class="form-group">
                            <label for="timeout">Timeout (ms)</label>
                            <input type="number" id="timeout" value="30000" min="1000" max="300000">
                        </div>
                        <div class="form-actions">
                            <button type="submit" class="btn btn-primary">Add Provider</button>
                            <button type="button" class="btn btn-secondary" onclick="hideProviderModal()">Cancel</button>
                        </div>
                    </form>
                </div>
            </div>
        </div>

        <!-- Loading Overlay -->
        <div id="loadingOverlay" class="loading-overlay" hidden>
            <div class="loading-spinner"></div>
            <div class="loading-text">Loading dashboard data...</div>
        </div>
    </div>

    <script src="/dashboard.js"></script>
</body>
</html>)";

    addResource("/dashboard.html", html, "text/html");
}

void ResourceLoader::initializeDashboardCSS() {
    const std::string css = R"(/* Aimux Professional Dashboard CSS */

/* CSS Custom Properties (Variables) */
:root[data-theme="light"] {
    --bg-primary: #ffffff;
    --bg-secondary: #f8fafc;
    --bg-tertiary: #f1f5f9;
    --bg-card: #ffffff;
    --text-primary: #1e293b;
    --text-secondary: #64748b;
    --text-tertiary: #94a3b8;
    --border-primary: #e2e8f0;
    --border-secondary: #cbd5e1;
    --accent-primary: #2563eb;
    --accent-secondary: #3b82f6;
    --success: #10b981;
    --warning: #f59e0b;
    --error: #ef4444;
    --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.05);
    --shadow-md: 0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1);
    --shadow-lg: 0 10px 15px -3px rgb(0 0 0 / 0.1), 0 4px 6px -4px rgb(0 0 0 / 0.1);
    --gradient-primary: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    --gradient-success: linear-gradient(135deg, #10b981 0%, #059669 100%);
    --gradient-warning: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
    --gradient-error: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
}

:root[data-theme="dark"] {
    --bg-primary: #0f172a;
    --bg-secondary: #1e293b;
    --bg-tertiary: #334155;
    --bg-card: #1e293b;
    --text-primary: #f1f5f9;
    --text-secondary: #cbd5e1;
    --text-tertiary: #94a3b8;
    --border-primary: #334155;
    --border-secondary: #475569;
    --accent-primary: #3b82f6;
    --accent-secondary: #60a5fa;
    --success: #10b981;
    --warning: #f59e0b;
    --error: #ef4444;
    --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.3);
    --shadow-md: 0 4px 6px -1px rgb(0 0 0 / 0.4), 0 2px 4px -2px rgb(0 0 0 / 0.3);
    --shadow-lg: 0 10px 15px -3px rgb(0 0 0 / 0.5), 0 4px 6px -4px rgb(0 0 0 / 0.3);
    --gradient-primary: linear-gradient(135deg, #3b82f6 0%, #8b5cf6 100%);
    --gradient-success: linear-gradient(135deg, #10b981 0%, #06b6d4 100%);
    --gradient-warning: linear-gradient(135deg, #f59e0b 0%, #f97316 100%);
    --gradient-error: linear-gradient(135deg, #ef4444 0%, #f43f5e 100%);
}

/* Reset and Base Styles */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
    background-color: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.6;
    transition: background-color 0.3s ease, color 0.3s ease;
}

/* App Container */
.app-container {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
}

/* Header */
.header {
    background-color: var(--bg-card);
    border-bottom: 1px solid var(--border-primary);
    box-shadow: var(--shadow-sm);
    position: sticky;
    top: 0;
    z-index: 100;
    transition: all 0.3s ease;
}

.header-content {
    max-width: 1200px;
    margin: 0 auto;
    padding: 1rem 2rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.logo {
    display: flex;
    align-items: center;
    gap: 0.75rem;
}

.logo-icon {
    color: var(--accent-primary);
}

.logo-text {
    font-size: 1.5rem;
    font-weight: 700;
    background: var(--gradient-primary);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
}

.header-controls {
    display: flex;
    align-items: center;
    gap: 1rem;
}

/* Theme Toggle */
.theme-toggle {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-primary);
    border-radius: 0.5rem;
    padding: 0.5rem;
    cursor: pointer;
    transition: all 0.3s ease;
    position: relative;
    width: 40px;
    height: 40px;
    display: flex;
    align-items: center;
    justify-content: center;
}

.theme-toggle:hover {
    background-color: var(--bg-tertiary);
    transform: scale(1.05);
}

.theme-icon {
    position: absolute;
    transition: opacity 0.3s ease, transform 0.3s ease;
}

:root[data-theme="light"] .light-icon {
    opacity: 1;
    transform: scale(1);
}

:root[data-theme="light"] .dark-icon {
    opacity: 0;
    transform: scale(0.5);
}

:root[data-theme="dark"] .light-icon {
    opacity: 0;
    transform: scale(0.5);
}

:root[data-theme="dark"] .dark-icon {
    opacity: 1;
    transform: scale(1);
}

/* Connection Status */
.connection-status {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    border-radius: 2rem;
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-primary);
    font-size: 0.875rem;
    transition: all 0.3s ease;
}

.status-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background-color: var(--success);
    animation: pulse 2s infinite;
}

.connection-status.disconnected .status-dot {
    background-color: var(--error);
    animation: none;
}

@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
}

/* Main Content */
.main-content {
    flex: 1;
    max-width: 1200px;
    margin: 0 auto;
    padding: 2rem;
    width: 100%;
}

/* Sections */
.section-title {
    font-size: 1.5rem;
    font-weight: 600;
    margin-bottom: 1.5rem;
    color: var(--text-primary);
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

/* Grid Layouts */
.overview-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 1.5rem;
    margin-bottom: 2rem;
}

.resources-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
    margin-bottom: 2rem;
}

.providers-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 1.5rem;
    margin-bottom: 2rem;
}

.metrics-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 1.5rem;
    margin-bottom: 2rem;
}

.config-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}

/* Cards */
.metric-card, .resource-card, .provider-card, .chart-card, .config-card {
    background-color: var(--bg-card);
    border: 1px solid var(--border-primary);
    border-radius: 0.75rem;
    padding: 1.5rem;
    box-shadow: var(--shadow-sm);
    transition: all 0.3s ease;
}

.metric-card:hover, .resource-card:hover, .provider-card:hover, .chart-card:hover, .config-card:hover {
    box-shadow: var(--shadow-md);
    transform: translateY(-2px);
}

/* Metric Cards */
.metric-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.metric-header h3 {
    font-size: 0.875rem;
    font-weight: 600;
    color: var(--text-secondary);
    text-transform: uppercase;
    letter-spacing: 0.025em;
}

.metric-icon {
    color: var(--accent-primary);
    opacity: 0.7;
}

.metric-value {
    font-size: 2.5rem;
    font-weight: 700;
    color: var(--text-primary);
    margin-bottom: 0.5rem;
    line-height: 1;
}

.metric-label {
    font-size: 0.875rem;
    color: var(--text-secondary);
}

/* Resource Charts */
.resource-chart-container {
    height: 200px;
    margin-bottom: 1rem;
}

.resource-info {
    text-align: center;
    font-size: 1.25rem;
    font-weight: 600;
    color: var(--text-primary);
}

/* Provider Cards */
.provider-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.provider-status {
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.status-indicator {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background-color: var(--success);
}

.status-indicator.unhealthy {
    background-color: var(--error);
}

.status-indicator.warning {
    background-color: var(--warning);
}

.provider-metrics {
    display: grid;
    gap: 0.75rem;
}

.metric-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem 0;
    border-bottom: 1px solid var(--border-primary);
}

.metric-label {
    font-size: 0.875rem;
    color: var(--text-secondary);
}

.metric-value-small {
    font-size: 0.875rem;
    font-weight: 600;
    color: var(--text-primary);
}

.provider-actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
}

/* Chart Cards */
.chart-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.chart-container {
    height: 300px;
    position: relative;
}

.time-range-select {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-primary);
    border-radius: 0.375rem;
    padding: 0.5rem;
    color: var(--text-primary);
    font-size: 0.875rem;
}

/* Configuration */
.config-content {
    display: flex;
    flex-direction: column;
    gap: 1rem;
}

.setting-item {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
}

.setting-item label {
    font-size: 0.875rem;
    font-weight: 500;
    color: var(--text-secondary);
}

.setting-item input,
.setting-item select {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-primary);
    border-radius: 0.375rem;
    padding: 0.5rem;
    color: var(--text-primary);
}

/* Buttons */
.btn {
    border: none;
    border-radius: 0.375rem;
    padding: 0.75rem 1.5rem;
    font-size: 0.875rem;
    font-weight: 500;
    cursor: pointer;
    transition: all 0.3s ease;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 0.5rem;
    text-decoration: none;
}

.btn-primary {
    background-color: var(--accent-primary);
    color: white;
}

.btn-primary:hover {
    background-color: var(--accent-secondary);
    transform: translateY(-1px);
    box-shadow: var(--shadow-md);
}

.btn-secondary {
    background-color: var(--bg-secondary);
    color: var(--text-primary);
    border: 1px solid var(--border-primary);
}

.btn-secondary:hover {
    background-color: var(--bg-tertiary);
    transform: translateY(-1px);
}

.btn-success {
    background-color: var(--success);
    color: white;
}

.btn-warning {
    background-color: var(--warning);
    color: white;
}

.btn-error {
    background-color: var(--error);
    color: white;
}

/* Modals */
.modal {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0, 0, 0, 0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
    opacity: 0;
    visibility: hidden;
    transition: all 0.3s ease;
}

.modal.show {
    opacity: 1;
    visibility: visible;
}

.modal-content {
    background-color: var(--bg-card);
    border-radius: 0.75rem;
    box-shadow: var(--shadow-lg);
    width: 90%;
    max-width: 500px;
    max-height: 90vh;
    overflow-y: auto;
    transform: scale(0.9);
    transition: transform 0.3s ease;
}

.modal.show .modal-content {
    transform: scale(1);
}

.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 1.5rem;
    border-bottom: 1px solid var(--border-primary);
}

.modal-header h3 {
    font-size: 1.25rem;
    font-weight: 600;
    color: var(--text-primary);
}

.modal-close {
    background: none;
    border: none;
    font-size: 1.5rem;
    color: var(--text-secondary);
    cursor: pointer;
    padding: 0;
    width: 2rem;
    height: 2rem;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 0.375rem;
    transition: all 0.3s ease;
}

.modal-close:hover {
    background-color: var(--bg-secondary);
    color: var(--text-primary);
}

.modal-body {
    padding: 1.5rem;
}

/* Forms */
.form-group {
    margin-bottom: 1rem;
}

.form-group label {
    display: block;
    margin-bottom: 0.5rem;
    font-size: 0.875rem;
    font-weight: 500;
    color: var(--text-secondary);
}

.form-group input,
.form-group select {
    width: 100%;
    background-color: var(--bg-secondary);
    border: 1px solid var(--border-primary);
    border-radius: 0.375rem;
    padding: 0.75rem;
    color: var(--text-primary);
    font-size: 0.875rem;
    transition: all 0.3s ease;
}

.form-group input:focus,
.form-group select:focus {
    outline: none;
    border-color: var(--accent-primary);
    box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.1);
}

.form-actions {
    display: flex;
    gap: 0.75rem;
    justify-content: flex-end;
    margin-top: 1.5rem;
}

/* Loading Overlay */
.loading-overlay {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0, 0, 0, 0.7);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    z-index: 2000;
}

.loading-spinner {
    width: 40px;
    height: 40px;
    border: 4px solid rgba(255, 255, 255, 0.3);
    border-radius: 50%;
    border-top-color: white;
    animation: spin 1s linear infinite;
}

.loading-text {
    color: white;
    margin-top: 1rem;
    font-size: 0.875rem;
}

@keyframes spin {
    to { transform: rotate(360deg); }
}

/* Responsive Design */
@media (max-width: 768px) {
    .header-content {
        padding: 1rem;
    }

    .main-content {
        padding: 1rem;
    }

    .overview-grid,
    .resources-grid,
    .providers-grid,
    .config-grid {
        grid-template-columns: 1fr;
        gap: 1rem;
    }

    .metric-value {
        font-size: 2rem;
    }

    .logo-text {
        font-size: 1.25rem;
    }

    .header-controls {
        gap: 0.5rem;
    }

    .modal-content {
        width: 95%;
        margin: 1rem;
    }
}

@media (max-width: 640px) {
    .header-content {
        flex-direction: column;
        gap: 1rem;
    }

    .section-title {
        font-size: 1.25rem;
    }

    .metric-value {
        font-size: 1.75rem;
    }

    .chart-container {
        height: 250px;
    }
}

/* Accessibility */
@media (prefers-reduced-motion: reduce) {
    * {
        animation-duration: 0.01ms !important;
        animation-iteration-count: 1 !important;
        transition-duration: 0.01ms !important;
    }
}

/* Focus styles for better keyboard navigation */
.btn:focus,
.theme-toggle:focus,
input:focus,
select:focus {
    outline: 2px solid var(--accent-primary);
    outline-offset: 2px;
}

/* High contrast mode support */
@media (prefers-contrast: high) {
    :root[data-theme="light"] {
        --border-primary: #000000;
        --text-secondary: #000000;
    }

    :root[data-theme="dark"] {
        --border-primary: #ffffff;
        --text-secondary: #ffffff;
    }
} )";

    addResource("/dashboard.css", css, "text/css");
}

void ResourceLoader::initializeDashboardJS() {
    const std::string js = R"(// Aimux Professional Dashboard JavaScript
// Console-based implementation for real-time updates

console.log('Aimux Dashboard loading...');

// Global dashboard object
var dashboard = {
    ws: null,
    charts: {},
    data: {
        providers: {},
        system: {},
        requests: {}
    },

    init: function() {
        console.log('Initializing dashboard...');
        this.startAutoRefresh();
        this.connectWebSocket();
    },

    connectWebSocket: function() {
        var protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        var wsUrl = protocol + '//' + window.location.host + '/ws';

        try {
            this.ws = new WebSocket(wsUrl);

            this.ws.onopen = function() {
                console.log('WebSocket connected');
            };

            this.ws.onmessage = function(event) {
                try {
                    var data = JSON.parse(event.data);
                    dashboard.updateUI(data);
                } catch (error) {
                    console.error('Error parsing data:', error);
                }
            };

            this.ws.onclose = function() {
                console.log('WebSocket disconnected');
            };

        } catch (error) {
            console.error('Failed to connect WebSocket:', error);
        }
    },

    updateUI: function(data) {
        this.data = data;

        // Update overview cards
        this.updateOverviewCards();

        // Update provider cards
        this.updateProviderCards();

        // Update system resources
        this.updateSystemResources();
    },

    updateOverviewCards: function() {
        var activeElement = document.getElementById('activeRequests');
        if (activeElement) {
            activeElement.textContent = (this.data.requests.per_second || 0).toFixed(1);
        }

        var totalElement = document.getElementById('totalRequests');
        if (totalElement) {
            var total = this.data.requests.total_today || 0;
            totalElement.textContent = this.formatNumber(total);
        }

        var successElement = document.getElementById('successRate');
        if (successElement) {
            successElement.textContent = this.calculateSuccessRate() + '%';
        }

        var uptimeElement = document.getElementById('uptime');
        if (uptimeElement) {
            var uptime = this.data.system.uptime_sec || 0;
            uptimeElement.textContent = this.formatDuration(uptime);
        }
    },

    updateProviderCards: function() {
        var providersGrid = document.getElementById('providersGrid');
        if (!providersGrid || !this.data.providers) return;

        providersGrid.innerHTML = '';

        for (var name in this.data.providers) {
            var provider = this.data.providers[name];
            var card = this.createProviderCard(name, provider);
            providersGrid.appendChild(card);
        }
    },

    createProviderCard: function(name, provider) {
        var card = document.createElement('div');
        card.className = 'provider-card';

        var isHealthy = provider.healthy !== false;
        var statusClass = provider.healthy === false ? 'unhealthy' : '';

        card.innerHTML =
            '<div class="provider-header">' +
            '<h3>' + this.capitalizeFirst(name) + '</h3>' +
            '<div class="provider-status">' +
            '<div class="status-indicator ' + statusClass + '"></div>' +
            '<span>' + (isHealthy ? 'Healthy' : 'Unhealthy') + '</span>' +
            '</div>' +
            '</div>' +
            '<div class="provider-metrics">' +
            '<div class="metric-row">' +
            '<span class="metric-label">Response Time</span>' +
            '<span class="metric-value-small">' + (provider.response_time_ms || 0) + 'ms</span>' +
            '</div>' +
            '<div class="metric-row">' +
            '<span class="metric-label">Success Rate</span>' +
            '<span class="metric-value-small">' + (provider.success_rate || 0) + '%</span>' +
            '</div>' +
            '<div class="metric-row">' +
            '<span class="metric-label">Requests/min</span>' +
            '<span class="metric-value-small">' + (provider.requests_per_min || 0) + '</span>' +
            '</div>' +
            '</div>' +
            '<div class="provider-actions">' +
            '<button class="btn btn-primary" onclick="dashboard.testProvider(\'' + name + '\')">Test</button>' +
            '<button class="btn btn-secondary" onclick="dashboard.toggleProvider(\'' + name + '\')">Toggle</button>' +
            '</div>';

        return card;
    },

    updateSystemResources: function() {
        var memoryUsage = this.data.system.memory_mb || 0;
        var cpuUsage = this.data.system.cpu_percent || 0;

        var memoryElement = document.getElementById('memoryUsage');
        if (memoryElement) {
            memoryElement.textContent = memoryUsage.toFixed(1) + ' MB';
        }

        var cpuElement = document.getElementById('cpuUsage');
        if (cpuElement) {
            cpuElement.textContent = cpuUsage.toFixed(1) + '%';
        }
    },

    formatNumber: function(num) {
        if (num >= 1000000) {
            return (num / 1000000).toFixed(1) + 'M';
        } else if (num >= 1000) {
            return (num / 1000).toFixed(1) + 'K';
        }
        return num.toString();
    },

    formatDuration: function(seconds) {
        if (seconds >= 3600) {
            var hours = Math.floor(seconds / 3600);
            var mins = Math.floor((seconds % 3600) / 60);
            return hours + 'h ' + mins + 'm';
        } else if (seconds >= 60) {
            var mins = Math.floor(seconds / 60);
            return mins + 'm';
        }
        return Math.floor(seconds) + 's';
    },

    capitalizeFirst: function(str) {
        return str.charAt(0).toUpperCase() + str.slice(1);
    },

    calculateSuccessRate: function() {
        if (!this.data.providers) return 0;

        var totalSuccess = 0;
        var providerCount = 0;

        for (var name in this.data.providers) {
            var provider = this.data.providers[name];
            if (provider.success_rate !== undefined) {
                totalSuccess += provider.success_rate;
                providerCount++;
            }
        }

        return providerCount > 0 ? Math.round(totalSuccess / providerCount) : 0;
    },

    startAutoRefresh: function() {
        // Auto-refresh is handled by WebSocket
    },

    testProvider: function(name) {
        console.log('Testing provider:', name);
        this.showNotification('Testing ' + name + '...', 'info');

        fetch('/test', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                provider: name,
                message: 'Hello from Aimux Dashboard!'
            })
        })
        .then(function(response) {
            return response.json();
        })
        .then(function(result) {
            if (result.success) {
                dashboard.showNotification(name + ' test successful', 'success');
            } else {
                dashboard.showNotification(name + ' test failed', 'error');
            }
        })
        .catch(function(error) {
            dashboard.showNotification('Test failed: ' + error.message, 'error');
        });
    },

    toggleProvider: function(name) {
        console.log('Toggle provider:', name);
        this.showNotification('Toggle ' + name + ' - feature coming soon', 'info');
    },

    showNotification: function(message, type) {
        console.log('[' + (type || 'info') + ']', message);

        // Create notification element
        var notification = document.createElement('div');
        notification.className = 'notification notification-' + (type || 'info');
        notification.textContent = message;

        Object.assign(notification.style, {
            position: 'fixed',
            top: '20px',
            right: '20px',
            padding: '1rem 1.5rem',
            borderRadius: '0.5rem',
            backgroundColor: type === 'error' ? '#ef4444' :
                             type === 'success' ? '#10b981' : '#2563eb',
            color: 'white',
            fontWeight: '500',
            zIndex: '3000',
            opacity: '0',
            transform: 'translateX(100%)',
            transition: 'all 0.3s ease'
        });

        document.body.appendChild(notification);

        setTimeout(function() {
            notification.style.opacity = '1';
            notification.style.transform = 'translateX(0)';
        }, 10);

        setTimeout(function() {
            notification.style.opacity = '0';
            notification.style.transform = 'translateX(100%)';
            setTimeout(function() {
                if (notification.parentNode) {
                    notification.parentNode.removeChild(notification);
                }
            }, 300);
        }, 3000);
    }
};

// Global functions
function showProviderModal() {
    console.log('Show provider modal');
    var modal = document.getElementById('providerModal');
    if (modal) modal.classList.add('show');
}

function hideProviderModal() {
    console.log('Hide provider modal');
    var modal = document.getElementById('providerModal');
    if (modal) modal.classList.remove('show');
}

function loadProviderConfig() {
    console.log('Load provider config');
    dashboard.showNotification('Configuration loading...', 'info');
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function() {
        dashboard.init();
    });
} else {
    dashboard.init();
}

console.log('Aimux Dashboard loaded'); )";

    addResource("/dashboard.js", js, "application/javascript");
}

} // namespace webui
} // namespace aimux