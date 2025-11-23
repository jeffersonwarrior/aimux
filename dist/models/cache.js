"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ModelCache = void 0;
const promises_1 = require("fs/promises");
const path_1 = require("path");
const info_1 = require("./info");
class ModelCache {
    cacheFile;
    cacheDurationMs;
    constructor(options) {
        this.cacheFile = options.cacheFile;
        this.cacheDurationMs = options.cacheDurationHours * 60 * 60 * 1000;
    }
    async isValid() {
        try {
            const stats = await (0, promises_1.stat)(this.cacheFile);
            const mtime = stats.mtime;
            const now = new Date();
            const age = now.getTime() - mtime.getTime();
            return age < this.cacheDurationMs;
        }
        catch (error) {
            // File doesn't exist or can't be accessed
            return false;
        }
    }
    async load() {
        if (!(await this.isValid())) {
            return [];
        }
        try {
            const data = await (0, promises_1.readFile)(this.cacheFile, 'utf-8');
            const cacheData = JSON.parse(data);
            const modelsData = cacheData.models || [];
            return modelsData.map((modelData) => new info_1.ModelInfoImpl(modelData));
        }
        catch (error) {
            console.error('Error loading cache:', error);
            return [];
        }
    }
    async save(models) {
        try {
            // Ensure parent directory exists
            const parentDir = (0, path_1.dirname)(this.cacheFile);
            await (0, promises_1.mkdir)(parentDir, { recursive: true });
            const cacheData = {
                models: models.map(model => model.toJSON()),
                timestamp: new Date().toISOString(),
                count: models.length,
            };
            const data = JSON.stringify(cacheData, null, 2);
            await (0, promises_1.writeFile)(this.cacheFile, data, 'utf-8');
            console.debug(`Cached ${models.length} models to ${this.cacheFile}`);
            return true;
        }
        catch (error) {
            console.error('Error saving cache:', error);
            return false;
        }
    }
    async clear() {
        try {
            await (0, promises_1.unlink)(this.cacheFile);
            console.debug('Cache cleared');
            return true;
        }
        catch (error) {
            console.error('Error clearing cache:', error);
            return false;
        }
    }
    async getInfo() {
        try {
            const stats = await (0, promises_1.stat)(this.cacheFile);
            const models = await this.load();
            return {
                exists: true,
                filePath: this.cacheFile,
                modifiedTime: stats.mtime.toISOString(),
                sizeBytes: stats.size,
                modelCount: models.length,
                isValid: await this.isValid(),
            };
        }
        catch (error) {
            return {
                exists: false,
                error: error.message,
            };
        }
    }
}
exports.ModelCache = ModelCache;
//# sourceMappingURL=cache.js.map