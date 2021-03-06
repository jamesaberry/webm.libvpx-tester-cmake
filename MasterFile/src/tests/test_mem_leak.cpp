#include "vpxt_test_declarations.h"

int test_mem_leak(int argc,
                  const char** argv,
                  const std::string &working_dir,
                  const std::string sub_folder_str,
                  int test_type,
                  int delete_ivf,
                  int artifact_detection)
{
    // Needs Debug.exe
    char *test_dir = "test_mem_leak";
    int input_ver = vpxt_check_arg_input(argv[1], argc);

    if (input_ver < 0)
        return vpxt_test_help(argv[1], 0);

    std::string input = argv[2];
    int mode = atoi(argv[3]);
    int bitrate = atoi(argv[4]);
    char mem_leak_exe[255];
    std::string mem_leak_exe_str = argv[5];
    snprintf(mem_leak_exe, 255, "%s", mem_leak_exe_str.c_str());
    std::string enc_format = argv[6];
    std::string dec_format = argv[7];

    //////////// Formatting Test Specific directory ////////////
    std::string cur_test_dir_str;
    std::string file_index_str;
    char main_test_dir_char[255] = "";
    char file_index_output_char[255] = "";

    if (initialize_test_directory(argc, argv, test_type, working_dir, test_dir,
        cur_test_dir_str, file_index_str, main_test_dir_char,
        file_index_output_char, sub_folder_str) == 11)
        return kTestErrFileMismatch;

    std::string exe_mem_leak_str;
    vpxt_folder_name(argv[0], &exe_mem_leak_str);

    /////////////////////////////////////////////////
    std::string mem_leak_check_dec_str = cur_test_dir_str + slashCharStr() +
        test_dir + "_decompression";
    vpxt_dec_format_append(mem_leak_check_dec_str, dec_format);
    /////////////////////////////////////////////////

    std::string mem_leak_check_enc_str = cur_test_dir_str + slashCharStr() +
        test_dir + "_compression";
    vpxt_enc_format_append(mem_leak_check_enc_str, enc_format);

    std::string enc_mem_leak_check_txt_str = cur_test_dir_str + slashCharStr() +
        test_dir + "_compression_memory_summary.txt";

    std::string dec_mem_leak_check_txt_str = cur_test_dir_str + slashCharStr() +
        test_dir + "_decompression_memory_summary.txt";

    std::string mem_leak_check_par_file_str = cur_test_dir_str + slashCharStr()
        + test_dir + "_compression_parameter_file.txt";

    std::string enc_mem_leak_check_cmd_str;
    std::string dec_mem_leak_check_cmd_str;

#if defined(_WIN32)
    {
        // compression
        enc_mem_leak_check_cmd_str = "\"\"" + exe_mem_leak_str + mem_leak_exe +
            "\" memcompress \"" + input + "\" \"" + mem_leak_check_enc_str +
            "\" 8 \"" + mem_leak_check_par_file_str + "\" 0 \"" +
            enc_mem_leak_check_txt_str + "\"\"";

        // decompression
        dec_mem_leak_check_cmd_str = "\"\"" + exe_mem_leak_str + mem_leak_exe +
            "\" memdecompress \"" + mem_leak_check_enc_str + "\" \"" +
            mem_leak_check_dec_str + "\" \"" + dec_mem_leak_check_txt_str +
            "\"\"";
    }
#else
    {
        // compression
        enc_mem_leak_check_cmd_str = "\"" + exe_mem_leak_str + mem_leak_exe +
            "\" memcompress \"" + input + "\" \"" + mem_leak_check_enc_str +
            "\" 8 \"" + mem_leak_check_par_file_str + "\" 0 \"" +
            enc_mem_leak_check_txt_str + "\"";

        // decompression
        dec_mem_leak_check_cmd_str = "\"" + exe_mem_leak_str + mem_leak_exe +
            "\" memdecompress \"" + mem_leak_check_enc_str + "\" \"" +
            mem_leak_check_dec_str + "\" \"" + dec_mem_leak_check_txt_str +
            "\"";
    }
#endif

    ///////////// Open Output File and Print Header ////////////
    std::string text_file_str = cur_test_dir_str + slashCharStr() + test_dir;
    FILE *fp;

    vpxt_open_output_file(test_type, text_file_str, fp);
    vpxt_print_header(argc, argv, main_test_dir_char, cur_test_dir_str,
        test_dir, test_type);

    VP8_CONFIG opt;
    vpxt_default_parameters(opt);

    /////////////////// Use Custom Settings ///////////////////
    if(vpxt_use_custom_settings(argv, argc, input_ver, fp, file_index_str,
        file_index_output_char, test_type, opt, bitrate)
        == kTestIndeterminate)
        return kTestIndeterminate;

    ///////////////// Make Sure Exe File Exists ///////////////
    if (!vpxt_file_exists_check(argv[5]))
    {
        tprintf(PRINT_BTH, "\nInput executable %s does not exist\n", argv[5]);

        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestIndeterminate;
    }

    ///////////////// Make Sure ivf File Exists ///////////////
    if (!vpxt_file_exists_check(argv[2]))
    {
        tprintf(PRINT_BTH, "\nInput encode file %s does not exist\n", argv[2]);

        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestIndeterminate;
    }

    /////////////////////////////////////////////////////////

    opt.target_bandwidth = bitrate ;

    // Run Test only (Runs Test, Sets up test to be run, or skips compresion of
    // files)
    if (test_type == kTestOnly)
    {
        // This test requires no preperation before a Test Only Run
    }
    else
    {
        fclose(fp);

        if ((fp = freopen(text_file_str.c_str(), "a+", stderr)) == NULL)
        {
            tprintf(PRINT_STD, "Cannot open out put file4.\n");
            exit(1);
        }

        fprintf(stderr, " ");

        opt.Mode = mode;
        vpxt_output_settings(mem_leak_check_par_file_str.c_str(), opt);
        vpxt_run_exe(enc_mem_leak_check_cmd_str.c_str());
        vpxt_run_exe(dec_mem_leak_check_cmd_str.c_str());
    }

    // Create Compression only stop test short.
    if (test_type == kCompOnly)
    {
        // Compression only run
        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestEncCreated;
    }

    int test_state = kTestPassed;
    tprintf(PRINT_BTH, "\n\nResults:\n\n");

    std::string enc_mem_results;
    std::string dec_mem_results;

    if (vpxt_check_mem_state(enc_mem_leak_check_txt_str, enc_mem_results) == -1)
    {
        vpxt_formated_print(RESPRT, "%s not found - Failed",
            enc_mem_leak_check_txt_str.c_str());
        test_state = kTestFailed;
    }
    else
    {
        if (enc_mem_results.compare(0, 24, "_currently Allocated= 0;") == 0)
        {
            vpxt_formated_print(RESPRT, "Compression Memory Currently "
                "Allocated == 0 - Passed");
            tprintf(PRINT_BTH, "\n");
        }
        else
        {
            vpxt_formated_print(RESPRT, "Compression Memory Currently "
                "Allocated != 0 - %s - Failed", enc_mem_results.c_str());
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
    }

    if (vpxt_check_mem_state(dec_mem_leak_check_txt_str, dec_mem_results) == -1)
    {
        vpxt_formated_print(RESPRT, "%s not found - Failed",
            enc_mem_leak_check_txt_str.c_str());
        test_state = kTestFailed;
    }
    else
    {
        if (dec_mem_results.compare(0, 24, "_currently Allocated= 0;") ==0)
        {
            vpxt_formated_print(RESPRT, "Decompression Memory Currently "
                "Allocated == 0 - Passed");
            tprintf(PRINT_BTH, "\n");
        }
        else
        {
            vpxt_formated_print(RESPRT, "Decompression Memory Currently "
                "Allocated != 0 - %s - Failed", dec_mem_results.c_str());
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
    }

    if (test_state == kTestPassed)
        tprintf(PRINT_BTH, "\nPassed\n");
    if (test_state == kTestFailed)
        tprintf(PRINT_BTH, "\nFailed\n");

    if (delete_ivf)
        vpxt_delete_files(2, mem_leak_check_dec_str.c_str(),
        mem_leak_check_enc_str.c_str());

    fclose(fp);
    record_test_complete(file_index_str, file_index_output_char, test_type);
    return test_state;
}