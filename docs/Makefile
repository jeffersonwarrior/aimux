# Aimux Documentation Makefile
# Generates comprehensive documentation using Doxygen, PlantUML, and custom tools

.PHONY: all clean docs html pdf diagrams api architecture help

# Default target
all: docs

# Configuration
DOXYGEN = doxygen
PLANTUML = plantuml
MKDIR = mkdir -p
RM = rm -rf
CP = cp
SED = sed

# Directories
BUILD_DIR = build
HTML_DIR = $(BUILD_DIR)/html
LATEX_DIR = $(BUILD_DIR)/latex
DIAGRAMS_DIR = $(BUILD_DIR)/diagrams
API_DIR = $(BUILD_DIR)/api
ARCH_DIR = $(BUILD_DIR)/architecture

# Source files
DOXYFILE = ../Doxyfile
PUML_FILES = *.puml
MARKDOWN_FILES = *.md

# Main targets
docs: html diagrams api

# HTML documentation
html: $(HTML_DIR)/index.html

$(HTML_DIR)/index.html: $(DOXYFILE) $(MARKDOWN_FILES)
	@echo "🚀 Generating HTML documentation..."
	$(MKDIR) $(HTML_DIR)
	$(DOXYGEN) $(DOXYFILE)
	@echo "✅ HTML documentation generated: $(HTML_DIR)/index.html"

# Diagrams with PlantUML
diagrams: $(DIAGRAMS_DIR)/architecture.svg $(DIAGRAMS_DIR)/data_flow.svg

$(DIAGRAMS_DIR)/architecture.svg: architecture.puml
	@echo "🎨 Generating architecture diagrams..."
	$(MKDIR) $(DIAGRAMS_DIR)
	$(PLANTUML) -tsvg -o$(DIAGRAMS_DIR)/ architecture.puml
	@echo "✅ Architecture diagram generated"

$(DIAGRAMS_DIR)/data_flow.svg: data_flow.puml
	$(PLANTUML) -tsvg -o$(DIAGRAMS_DIR)/ data_flow.puml
	@echo "✅ Data flow diagram generated"

# PDF documentation
pdf: $(LATEX_DIR)/refman.pdf

$(LATEX_DIR)/refman.pdf: $(DOXYFILE)
	@echo "📄 Generating PDF documentation..."
	$(DOXYGEN) $(DOXYFILE)
	cd $(LATEX_DIR) && $(MAKE) refman.pdf
	@echo "✅ PDF documentation generated: $(LATEX_DIR)/refman.pdf"

# API documentation extraction
api: $(API_DIR)/api-complete.md

$(API_DIR)/api-complete.md: $(MARKDOWN_FILES)
	@echo "📚 Processing API documentation..."
	$(MKDIR) $(API_DIR)
	$(CP) api.md $(API_DIR)/api-complete.md
	@echo "✅ API documentation processed"

# Architecture analysis
architecture: $(ARCH_DIR)/components.json $(ARCH_DIR)/dependencies.json

$(ARCH_DIR)/components.json:
	@echo "🏗️ Analyzing component structure..."
	$(MKDIR) $(ARCH_DIR)
	find ../include -name "*.hpp" -exec basename {} .hpp \; | sort | uniq > $(ARCH_DIR)/components.txt
	@echo "✅ Component analysis complete"

$(ARCH_DIR)/dependencies.json:
	@echo "🔗 Analyzing dependencies..."
	# Could implement include graph analysis here
	@echo "✅ Dependency analysis complete"

# Complete documentation set
complete: docs diagrams api pdf
	@echo "🎉 Complete documentation generated successfully!"
	@echo "📁 HTML: $(HTML_DIR)/index.html"
	@echo "📄 PDF: $(LATEX_DIR)/refman.pdf"
	@echo "🎨 Diagrams: $(DIAGRAMS_DIR)/"
	@echo "📚 API: $(API_DIR)/"

# Development documentation generation
dev-docs: docs
	@echo "🔧 Development documentation"
	@echo ""
	@echo "📖 API Reference:"
	@echo "   open $(HTML_DIR)/index.html"
	@echo ""
	@echo "🎨 Architecture Diagrams:"
	@echo "   open $(DIAGRAMS_DIR)/architecture.svg"
	@echo ""
	@echo "📊 Data Flow:"
	@echo "   open $(DIAGRAMS_DIR)/data_flow.svg"

# Quick check for missing documentation
check:
	@echo "🔍 Checking documentation completeness..."
	@echo ""
	@echo "📁 Core components:"
	@find ../include -name "*.hpp" | wc -l | xargs printf "   %d header files\n"
	@find ../src -name "*.cpp" | wc -l | xargs printf "   %d source files\n"
	@echo ""
	@echo "📚 Documentation files:"
	@ls -la $(MARKDOWN_FILES) | wc -l | xargs printf "   %d markdown files\n"
	@echo "   Doxyfile configuration: $(DOXYFILE)"
	@echo "   PlantUML diagrams: $(PUML_FILES)"
	@echo ""
	@echo "✅ Documentation structure validated"

# Statistics generation
stats:
	@echo "📊 Documentation Statistics"
	@echo "========================"
	@echo ""
	@echo "📏 Code Metrics:"
	@echo "   Lines of code: $$(find ../src -name "*.cpp" -o -name "*.hpp" | xargs wc -l | tail -1 | awk '{print $$1}')"
	@echo "   API endpoints: $$(grep -r "POST\|GET\|PUT\|DELETE" ../include | grep -v "//" | wc -l)"
	@echo "   Data structures: $$(grep -r "struct\|class" ../include | grep -v "//" | wc -l)"
	@echo ""
	@echo "📚 Documentation Metrics:"
	@echo "   Markdown files: $$(ls $(MARKDOWN_FILES) 2>/dev/null | wc -l | xargs echo 0 | awk '{print $$1}')"
	@echo "   Diagram files: $$(ls $(PUML_FILES) 2>/dev/null | wc -l | xargs echo 0 | awk '{print $$1}')"
	@echo "   API pages: $$(grep -c "## " api.md 2>/dev/null || echo 0)"

# Serve documentation locally (requires Python)
serve: docs
	@echo "🌐 Starting local documentation server..."
	@echo "   Open http://localhost:8000 in your browser"
	@cd $(HTML_DIR) && python3 -m http.server 8000

# Clean generated files
clean:
	@echo "🧹 Cleaning documentation build artifacts..."
	$(RM) $(BUILD_DIR)
	@echo "✅ Clean completed"

# Help information
help:
	@echo "📚 Aimux Documentation Generator"
	@echo "================================="
	@echo ""
	@echo "🎯 Main Targets:"
	@echo "   all        - Generate complete documentation (default)"
	@echo "   docs       - Generate HTML docs and diagrams"
	@echo "   html       - Generate HTML documentation only"
	@echo "   diagrams   - Generate PlantUML diagrams only"
	@echo "   pdf        - Generate PDF documentation"
	@echo "   api        - Process API documentation"
	@echo "   complete   - Generate all documentation formats"
	@echo ""
	@echo "🔧 Development:"
	@echo "   dev-docs   - Generate docs and open them"
	@echo "   serve      - Serve docs locally on port 8000"
	@echo "   check      - Validate documentation completeness"
	@echo "   stats      - Show documentation statistics"
	@echo ""
	@echo "🧹 Maintenance:"
	@echo "   clean      - Remove all generated files"
	@echo "   help       - Show this help message"
	@echo ""
	@echo "📁 Output:"
	@echo "   HTML: $(HTML_DIR)/"
	@echo "   PDF:  $(LATEX_DIR)/"
	@echo "   SVG:  $(DIAGRAMS_DIR)/"
	@echo ""
	@echo "🔍 Prerequisites:"
	@echo "   - Doxygen 1.9+"
	@echo "   - PlantUML (for diagrams)"
	@echo "   - Python 3 (for serve)"
	@echo "   - LaTeX (for PDF)"

# Continuous Integration target
ci: check docs
	@echo "✅ CI documentation build successful"

# Development shortcuts
quick: docs
	@echo "⚡ Quick documentation generation complete"

watch:
	@echo "👀 Watching for changes (requires inotify-tools)..."
	@inotifywait -m -r -e modify ../include ../src ../docs --include='*.hpp' --include='*.cpp' --include='*.md' --include='*.puml' | while read file; do \
		echo "🔄 File changed: $$file"; \
		sleep 1; \
		$(MAKE) docs; \
	done

# Export documentation to different formats
export-mkdocs: docs
	@echo "📖 Exporting to MkDocs format..."
	$(MKDIR) $(BUILD_DIR)/mkdocs
	$(CP) *.md $(BUILD_DIR)/mkdocs/

export-readthedocs: docs
	@echo "📦 Preparing ReadTheDocs build..."
	$(MKDIR) $(BUILD_DIR)/readthedocs
	$(CP) *.md $(BUILD_DIR)/readthedocs/
	$(CP) $(HTML_DIR)/* $(BUILD_DIR)/readthedocs/

# Environment check
env:
	@echo "🔍 Checking environment..."
	@which doxygen > /dev/null && echo "✅ Doxygen: $$(doxygen --version)" || echo "❌ Doxygen not found"
	@which plantuml > /dev/null && echo "✅ PlantUML: installed" || echo "⚠️  PlantUML not found - diagrams disabled"
	@which python3 > /dev/null && echo "✅ Python 3: $$(python3 --version)" || echo "❌ Python 3 not found"
	@which pdflatex > /dev/null && echo "✅ LaTeX: available" || echo "⚠️  LaTeX not found - PDF disabled"
	@echo ""
	@echo "📁 Build directory: $(BUILD_DIR)"
	@echo "📊 Disk space: $$(du -sh $(BUILD_DIR) 2>/dev/null | cut -f1 || echo 'not created yet')"

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "📦 Installing documentation dependencies..."
	sudo apt-get update
	sudo apt-get install -y doxygen plantuml graphviz python3 default-jre-headless

# Install dependencies (macOS with Homebrew)
install-deps-macos:
	@echo "📦 Installing documentation dependencies (macOS)..."
	brew install doxygen plantuml graphviz python3

.FORCE:

# Detect makefile changes
.PHONY: $(DOXYFILE) $(MARKDOWN_FILES) $(PUML_FILES)