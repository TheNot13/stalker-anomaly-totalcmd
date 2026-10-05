void ReadFAT() {
    virtual_files.clear();
    
    std::string db_path = GetDbPath();
    if (db_path.empty()) {
        virtual_files.push_back("ERROR_NO_DBPATH_IN_INI.txt");
        virtual_files.push_back(GetIniPath());
        return;
    }
    
    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        virtual_files.push_back("ERROR_FILE_NOT_FOUND.txt");
        
        // Делаем путь безопасным для отображения в виде файла
        std::string safe_path = db_path;
        std::replace(safe_path.begin(), safe_path.end(), '\\', '_');
        std::replace(safe_path.begin(), safe_path.end(), ':', '_');
        std::replace(safe_path.begin(), safe_path.end(), '"', '_');
        
        virtual_files.push_back("PATH_WAS__" + safe_path + ".txt");
        return;
    }
    
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);
    
    uint32_t fat_offset = 0;
    uint32_t compressed_fat_size = 0;
    
    if (version == 666) {
        uint32_t ini_size = 0;
        file.read(reinterpret_cast<char*>(&ini_size), 4);
        file.seekg(ini_size, std::ios::cur); 
        
        uint32_t val1 = 0, val2 = 0;
        file.read(reinterpret_cast<char*>(&val1), 4);
        file.read(reinterpret_cast<char*>(&val2), 4);
        
        fat_offset = val2;
        compressed_fat_size = static_cast<uint32_t>(file_size) - fat_offset;
    } else {
        virtual_files.push_back("ERROR_UNSUPPORTED_VERSION.txt");
        return;
    }

    file.seekg(fat_offset, std::ios::beg);
    std::vector<uint8_t> comp_fat(compressed_fat_size);
    file.read(reinterpret_cast<char*>(comp_fat.data()), compressed_fat_size);
    file.close();

    int max_decomp_size = 20 * 1024 * 1024; 
    std::vector<uint8_t> decomp_fat(max_decomp_size);
    
    int decomp_size = LZ4_decompress_safe(
        reinterpret_cast<const char*>(comp_fat.data()), 
        reinterpret_cast<char*>(decomp_fat.data()), 
        compressed_fat_size, 
        max_decomp_size
    );
    
    if (decomp_size < 0) {
        virtual_files.push_back("ERROR_DECOMPRESS_LZ4_" + std::to_string(decomp_size) + ".txt");
        return;
    }
    
    uint32_t fat_ptr = 0;
    int count = 0;
    
    while (fat_ptr < (uint32_t)decomp_size) {
        if (fat_ptr + 12 > (uint32_t)decomp_size) break;
        
        uint32_t size_real = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t size_comp = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        uint32_t crc = *(uint32_t*)(decomp_fat.data() + fat_ptr); fat_ptr += 4;
        
        std::string name = reinterpret_cast<char*>(decomp_fat.data() + fat_ptr);
        fat_ptr += name.length() + 1; 
        
        std::replace(name.begin(), name.end(), '\\', '_');
        std::replace(name.begin(), name.end(), '/', '_');
        
        if (count < 100) {
            virtual_files.push_back(name);
        }
        count++;
    }
    
    virtual_files.push_back("SUCCESS_TOTAL_FILES_" + std::to_string(count) + ".txt");
}
