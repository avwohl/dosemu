/*
 * ContentView.swift - Main UI with config profiles and device settings
 */

import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @StateObject private var viewModel = EmulatorViewModel()
    @State private var fontSize: CGFloat = 14
    @State private var showingSettings = false
    @State private var showingNewConfig = false
    @State private var showingSaveAs = false
    @State private var newConfigName = ""

    var body: some View {
        NavigationView {
            if viewModel.isRunning {
                runningView
            } else {
                settingsView
            }
        }
        .navigationViewStyle(.stack)
        .alert("Error", isPresented: $viewModel.showingError) {
            Button("OK") {}
        } message: {
            Text(viewModel.errorMessage)
        }
        .alert("Disk May Be Overwritten", isPresented: $viewModel.showingManifestWriteWarning) {
            Button("OK") {}
        } message: {
            Text("This disk was downloaded from the catalog and may be replaced when the catalog is updated. Any changes you save could be lost.\n\nTo keep changes permanently, use Save Disk to copy to your own file.")
        }
        .fileImporter(isPresented: $viewModel.showingDiskPicker, allowedContentTypes: [.data]) { result in
            viewModel.handleDiskImport(result.map { [$0] })
        }
        .fileExporter(isPresented: $viewModel.showingDiskExporter, document: viewModel.exportDocument, contentType: .data, defaultFilename: "disk.img") { result in
            viewModel.handleExportResult(result)
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.willResignActiveNotification)) { _ in
            viewModel.saveDisksOnBackground()
        }
    }

    // MARK: - Running View

    var runningView: some View {
        VStack(spacing: 0) {
            HStack {
                Text(viewModel.config.name)
                    .font(.headline)
                Spacer()
                if !viewModel.statusText.isEmpty {
                    Text(viewModel.statusText)
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Button("Stop") { viewModel.stop() }
                    .foregroundColor(.red)
            }
            .padding(.horizontal)
            .padding(.vertical, 6)

            TerminalWithToolbar(
                cells: $viewModel.terminalCells,
                cursorRow: $viewModel.cursorRow,
                cursorCol: $viewModel.cursorCol,
                shouldFocus: $viewModel.terminalShouldFocus,
                onKeyInput: { viewModel.sendKey($0) },
                onSetControlify: { viewModel.setControlify($0) },
                onMouseUpdate: { x, y, btn in viewModel.sendMouseUpdate(x: x, y: y, buttons: btn) },
                isControlifyActive: viewModel.isControlifyActive,
                rows: viewModel.terminalRows,
                cols: viewModel.terminalCols,
                fontSize: fontSize
            )
        }
    }

    // MARK: - Settings View

    var settingsView: some View {
        Form {
            configProfileSection
            displaySection
            peripheralsSection
            diskSection
            urlDownloadSection
            catalogSection
            bootSection
            preferencesSection
            aboutSection
        }
        .navigationTitle("DOSEmu")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button("Start") { viewModel.start() }
                    .disabled(viewModel.floppyAPath == nil && viewModel.hddCPath == nil)
            }
        }
        .alert("New Configuration", isPresented: $showingNewConfig) {
            TextField("Name", text: $newConfigName)
            Button("Create") {
                if !newConfigName.isEmpty {
                    _ = viewModel.configManager.addConfig(name: newConfigName)
                    newConfigName = ""
                }
            }
            Button("Cancel", role: .cancel) { newConfigName = "" }
        }
        .alert("Save As", isPresented: $showingSaveAs) {
            TextField("Name", text: $newConfigName)
            Button("Save") {
                if !newConfigName.isEmpty {
                    _ = viewModel.configManager.duplicateConfig(viewModel.config, name: newConfigName)
                    newConfigName = ""
                }
            }
            Button("Cancel", role: .cancel) { newConfigName = "" }
        }
    }

    // MARK: - Config Profile Section

    var configProfileSection: some View {
        Section("Configuration Profile") {
            Picker("Profile", selection: Binding(
                get: { viewModel.configManager.activeConfigId ?? UUID() },
                set: { id in
                    if let cfg = viewModel.configManager.configs.first(where: { $0.id == id }) {
                        viewModel.configManager.selectConfig(cfg)
                    }
                }
            )) {
                ForEach(viewModel.configManager.configs) { cfg in
                    Text(cfg.name).tag(cfg.id)
                }
            }

            HStack {
                Button("New") { showingNewConfig = true }
                Spacer()
                Button("Save As") { showingSaveAs = true }
                Spacer()
                Button("Delete", role: .destructive) {
                    if viewModel.configManager.configs.count > 1 {
                        viewModel.configManager.deleteConfig(viewModel.config)
                    }
                }
                .disabled(viewModel.configManager.configs.count <= 1)
            }

            HStack {
                Text("Name")
                Spacer()
                TextField("Config Name", text: Binding(
                    get: { viewModel.config.name },
                    set: { name in
                        var cfg = viewModel.config
                        cfg.name = name
                        viewModel.configManager.updateConfig(cfg)
                    }
                ))
                .multilineTextAlignment(.trailing)
                .frame(maxWidth: 200)
            }
        }
    }

    // MARK: - Display Section

    var displaySection: some View {
        Section("Display") {
            Picker("Adapter", selection: Binding(
                get: { viewModel.config.displayAdapter },
                set: { val in
                    var cfg = viewModel.config
                    cfg.displayAdapter = val
                    viewModel.configManager.updateConfig(cfg)
                }
            )) {
                Text("CGA").tag(0)
                Text("MDA").tag(1)
                Text("Hercules").tag(2)
                Text("CGA + MDA").tag(3)
            }
        }
    }

    // MARK: - Peripherals Section

    var peripheralsSection: some View {
        Section("Peripherals") {
            Toggle("Mouse", isOn: Binding(
                get: { viewModel.config.mouseEnabled },
                set: { val in
                    var cfg = viewModel.config
                    cfg.mouseEnabled = val
                    viewModel.configManager.updateConfig(cfg)
                }
            ))

            Toggle("PC Speaker", isOn: Binding(
                get: { viewModel.config.speakerEnabled },
                set: { val in
                    var cfg = viewModel.config
                    cfg.speakerEnabled = val
                    viewModel.configManager.updateConfig(cfg)
                }
            ))

            Picker("Sound Card", selection: Binding(
                get: { viewModel.config.soundCard },
                set: { val in
                    var cfg = viewModel.config
                    cfg.soundCard = val
                    viewModel.configManager.updateConfig(cfg)
                }
            )) {
                Text("None").tag(0)
                Text("AdLib").tag(1)
                Text("Sound Blaster").tag(2)
            }

            Toggle("CD-ROM", isOn: Binding(
                get: { viewModel.config.cdromEnabled },
                set: { val in
                    var cfg = viewModel.config
                    cfg.cdromEnabled = val
                    viewModel.configManager.updateConfig(cfg)
                }
            ))

            Picker("CPU Speed", selection: Binding(
                get: { viewModel.config.speedMode },
                set: { val in
                    var cfg = viewModel.config
                    cfg.speedMode = val
                    viewModel.configManager.updateConfig(cfg)
                }
            )) {
                Text("Full Speed").tag(0)
                Text("IBM PC (4.77 MHz)").tag(1)
                Text("IBM AT (8 MHz)").tag(2)
                Text("Turbo (25 MHz)").tag(3)
            }
        }
    }

    // MARK: - Disk Section

    var diskSection: some View {
        Section("Disks") {
            diskRow(label: "Floppy A:", path: viewModel.floppyAPath, unit: 0)
            diskRow(label: "Floppy B:", path: viewModel.floppyBPath, unit: 1)
            diskRow(label: "Hard Disk C:", path: viewModel.hddCPath, unit: 0x80)
            diskRow(label: "CD-ROM:", path: viewModel.isoPath, unit: 0xE0)

            Menu("Create Blank Disk") {
                Button("360 KB Floppy") { viewModel.createBlankFloppy(sizeKB: 360) }
                Button("720 KB Floppy") { viewModel.createBlankFloppy(sizeKB: 720) }
                Button("1.44 MB Floppy") { viewModel.createBlankFloppy(sizeKB: 1440) }
                Divider()
                Button("10 MB HDD") { viewModel.createBlankHDD(sizeMB: 10) }
                Button("20 MB HDD") { viewModel.createBlankHDD(sizeMB: 20) }
                Button("32 MB HDD") { viewModel.createBlankHDD(sizeMB: 32) }
                Button("64 MB HDD") { viewModel.createBlankHDD(sizeMB: 64) }
            }
        }
    }

    func diskRow(label: String, path: URL?, unit: Int) -> some View {
        HStack {
            Text(label)
            Spacer()
            if let p = path {
                Text(p.lastPathComponent)
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .lineLimit(1)
                Button(role: .destructive) { viewModel.removeDisk(unit) } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.red)
                }
                .buttonStyle(.plain)
            } else {
                Button("Load") { viewModel.loadDisk(unit) }
            }
        }
    }

    // MARK: - URL Download Section

    var urlDownloadSection: some View {
        Section("Download from URL") {
            HStack {
                TextField("https://...", text: $viewModel.urlInput)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .keyboardType(.URL)
            }
            if viewModel.urlDownloading {
                ProgressView(value: viewModel.urlDownloadProgress)
            } else {
                HStack {
                    Button("Floppy A:") { viewModel.downloadFromURL(toDrive: 0) }
                        .disabled(viewModel.urlInput.isEmpty)
                    Spacer()
                    Button("HDD C:") { viewModel.downloadFromURL(toDrive: 0x80) }
                        .disabled(viewModel.urlInput.isEmpty)
                    Spacer()
                    Button("CD-ROM") { viewModel.downloadFromURL(toDrive: 0xE0) }
                        .disabled(viewModel.urlInput.isEmpty)
                }
                .font(.caption)
            }
        }
    }

    // MARK: - Catalog Section

    var catalogSection: some View {
        Section("Disk Catalog") {
            if viewModel.catalogLoading {
                HStack { ProgressView(); Text("Loading catalog...").foregroundColor(.secondary) }
            } else if let err = viewModel.catalogError {
                HStack {
                    Text(err).foregroundColor(.red).font(.caption)
                    Spacer()
                    Button("Retry") { viewModel.fetchDiskCatalog() }
                }
            } else if viewModel.diskCatalog.isEmpty {
                Text("No disk images available")
                    .foregroundColor(.secondary)
            } else {
                ForEach(viewModel.diskCatalog) { disk in
                    catalogRow(disk)
                }
            }
        }
    }

    func catalogRow(_ disk: DownloadableDisk) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(disk.name).font(.headline)
                Spacer()
                Text(disk.type.label)
                    .font(.caption2)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(disk.type == .iso ? Color.purple.opacity(0.15) : disk.type == .hdd ? Color.orange.opacity(0.15) : Color.blue.opacity(0.15))
                    .cornerRadius(4)
                Text(disk.formattedSize).font(.caption).foregroundColor(.secondary)
            }
            Text(disk.description).font(.caption).foregroundColor(.secondary)

            let state = viewModel.downloadStates[disk.filename] ?? .notDownloaded
            switch state {
            case .notDownloaded:
                Button("Download") { viewModel.downloadDisk(disk) }
                    .font(.caption)
            case .downloading(let progress):
                ProgressView(value: progress)
            case .downloaded:
                HStack(spacing: 12) {
                    switch disk.type {
                    case .floppy:
                        Button("Use as A:") { viewModel.useCatalogDisk(disk, forDrive: 0) }
                        Button("Use as B:") { viewModel.useCatalogDisk(disk, forDrive: 1) }
                    case .hdd:
                        Button("Use as C:") { viewModel.useCatalogDisk(disk, forDrive: 0x80) }
                    case .iso:
                        Button("Use as CD-ROM") { viewModel.useCatalogDisk(disk, forDrive: 0xE0) }
                    }
                }
                .font(.caption)
            case .error(let msg):
                HStack {
                    Text(msg).foregroundColor(.red).font(.caption)
                    Button("Retry") { viewModel.downloadDisk(disk) }.font(.caption)
                }
            }
        }
        .padding(.vertical, 2)
    }

    // MARK: - Boot Section

    var bootSection: some View {
        Section("Boot") {
            Picker("Boot Drive", selection: $viewModel.bootDrive) {
                Text("Floppy A:").tag(0)
                Text("Hard Disk C:").tag(0x80)
            }

            Button(action: { viewModel.start() }) {
                HStack {
                    Spacer()
                    Text("Start Emulator")
                        .font(.headline)
                    Spacer()
                }
            }
            .disabled(viewModel.floppyAPath == nil && viewModel.hddCPath == nil)
        }
    }

    // MARK: - Preferences Section

    var preferencesSection: some View {
        Section("Preferences") {
            Toggle("Warn on Catalog Disk Writes", isOn: Binding(
                get: { viewModel.warnManifestWrites },
                set: { viewModel.warnManifestWrites = $0 }
            ))
            Text("Show a warning when the emulator writes to a disk downloaded from the catalog. Changes may be lost when the catalog is updated.")
                .font(.caption)
                .foregroundColor(.secondary)
        }
    }

    // MARK: - About Section

    var aboutSection: some View {
        Section {
            NavigationLink(destination: AboutView()) {
                Text("About DOSEmu")
            }
        }
    }
}

// MARK: - About View

struct AboutView: View {
    private var appVersion: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0.1.0"
    }

    private var buildNumber: String {
        Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
    }

    var body: some View {
        Form {
            Section {
                VStack(spacing: 12) {
                    Image("AppIcon")
                        .resizable()
                        .frame(width: 80, height: 80)
                        .cornerRadius(18)
                        .accessibilityHidden(true)

                    Text("DOSEmu")
                        .font(.title.bold())

                    Text("Version \(appVersion) (Build \(buildNumber))")
                        .font(.subheadline)
                        .foregroundColor(.secondary)

                    Text("An 8088 IBM PC emulator for iOS and Mac.")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .multilineTextAlignment(.center)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 12)
            }

            Section("Links") {
                Link(destination: URL(string: "https://github.com/avwohl/dosemu")!) {
                    HStack {
                        Label("Source Code on GitHub", systemImage: "chevron.left.forwardslash.chevron.right")
                        Spacer()
                        Image(systemName: "arrow.up.right.square")
                            .foregroundColor(.secondary)
                    }
                }

                Link(destination: URL(string: "https://github.com/avwohl/dosemu/issues")!) {
                    HStack {
                        Label("Report an Issue", systemImage: "exclamationmark.bubble")
                        Spacer()
                        Image(systemName: "arrow.up.right.square")
                            .foregroundColor(.secondary)
                    }
                }

                Link(destination: URL(string: "https://www.freedos.org")!) {
                    HStack {
                        Label("FreeDOS Project", systemImage: "desktopcomputer")
                        Spacer()
                        Image(systemName: "arrow.up.right.square")
                            .foregroundColor(.secondary)
                    }
                }
            }

            Section("Acknowledgments") {
                Text("FreeDOS is a free, open-source DOS-compatible operating system that can be used with this emulator. Visit freedos.org to download disk images.")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
        .navigationTitle("About")
        .navigationBarTitleDisplayMode(.inline)
    }
}
