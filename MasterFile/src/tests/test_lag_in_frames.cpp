#include "vpxt_test_declarations.h"

int test_lag_in_frames(int argc,
                       const char** argv,
                       const std::string &working_dir,
                       const std::string sub_folder_str,
                       int test_type,
                       int delete_ivf,
                       int artifact_detection)
{
    char *comp_out_str = "Lag In Frames";
    char *test_dir = "test_lag_in_frames";
    int input_ver = vpxt_check_arg_input(argv[1], argc);

    if (input_ver < 0)
        return vpxt_test_help(argv[1], 0);

    std::string input = argv[2];
    int mode = atoi(argv[3]);
    int bitrate = atoi(argv[4]);;
    int lag_in_frames_1_val = atoi(argv[5]);
    int lag_in_frames_2_val = atoi(argv[6]);
    std::string enc_format = argv[7];

    int speed = 0;

    //////////// Formatting Test Specific directory ////////////

    std::string cur_test_dir_str;
    std::string file_index_str;
    char main_test_dir_char[255] = "";
    char file_index_output_char[255] = "";

    if (initialize_test_directory(argc, argv, test_type, working_dir, test_dir,
        cur_test_dir_str, file_index_str, main_test_dir_char,
        file_index_output_char, sub_folder_str) == 11)
        return kTestErrFileMismatch;

    char lag_in_frames_buff[255];

    int lag_in_frames_0_art_det = artifact_detection;
    int lag_in_frames_1_art_det = artifact_detection;
    int lag_in_frames_2_art_det = artifact_detection;

    std::string lag_in_frames_0 = cur_test_dir_str + slashCharStr() +
        "test_lag_in_frames_compression_0";
    vpxt_enc_format_append(lag_in_frames_0, enc_format);

    vpxt_itoa_custom(lag_in_frames_1_val, lag_in_frames_buff, 10);
    std::string lag_in_frames_1 = cur_test_dir_str + slashCharStr() + test_dir +
        "_compression_";
    lag_in_frames_1 += lag_in_frames_buff;
    vpxt_enc_format_append(lag_in_frames_1, enc_format);

    vpxt_itoa_custom(lag_in_frames_2_val, lag_in_frames_buff, 10);
    std::string lag_in_frames_2 = cur_test_dir_str + slashCharStr() + test_dir +
        "_compression_";
    lag_in_frames_2 += *lag_in_frames_buff;
    vpxt_enc_format_append(lag_in_frames_2, enc_format);

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

    opt.target_bandwidth = bitrate;

    if (lag_in_frames_1_val > 25 || lag_in_frames_2_val > 25 ||
        lag_in_frames_2_val < 0 || lag_in_frames_1_val < 0)
    {
        tprintf(PRINT_BTH, "\nLag in Frames settings must be between 0 and "
            "25.\n");

        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestIndeterminate;
    }

    // Run Test only (Runs Test, Sets up test to be run, or skips compresion of
    // files)
    if (test_type == kTestOnly)
    {
        // This test requires no preperation before a Test Only Run
    }
    else
    {
        opt.Mode = mode;

        opt.allow_lag = 0;
        opt.lag_in_frames = lag_in_frames_1_val;

        if (vpxt_compress(input.c_str(), lag_in_frames_0.c_str(), speed,
            bitrate, opt, comp_out_str, 0, 1, enc_format, kSetConfigOff) == -1)
        {
            fclose(fp);
            record_test_complete(file_index_str, file_index_output_char,
                test_type);
            return kTestIndeterminate;
        }

        opt.allow_lag = 1;
        opt.lag_in_frames = lag_in_frames_1_val;

        if (vpxt_compress(input.c_str(), lag_in_frames_1.c_str(), speed,
            bitrate, opt, comp_out_str, lag_in_frames_2_val, 1, enc_format,
            kSetConfigOff) == -1)
        {
            fclose(fp);
            record_test_complete(file_index_str, file_index_output_char,
                test_type);
            return kTestIndeterminate;
        }

        opt.allow_lag = 1;
        opt.lag_in_frames = lag_in_frames_2_val;

        if (vpxt_compress(input.c_str(), lag_in_frames_2.c_str(), speed,
            bitrate, opt, comp_out_str, lag_in_frames_2_val, 1, enc_format,
            kSetConfigOff) == -1)
        {
            fclose(fp);
            record_test_complete(file_index_str, file_index_output_char,
                test_type);
            return kTestIndeterminate;
        }
    }

    if (test_type == kCompOnly)
    {
        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestEncCreated;
    }

    double lag_in_frames_0_psnr = vpxt_psnr(input.c_str(),
        lag_in_frames_0.c_str(), 0, PRINT_BTH, 1, 0, 0, 0, NULL,
        lag_in_frames_0_art_det);
    double lag_in_frames_1_psnr = vpxt_psnr(input.c_str(),
        lag_in_frames_1.c_str(), 0, PRINT_BTH, 1, 0, 0, 0, NULL,
        lag_in_frames_1_art_det);
    double lag_in_frames_2_psnr = vpxt_psnr(input.c_str(),
        lag_in_frames_2.c_str(), 0, PRINT_BTH, 1, 0, 0, 0, NULL,
        lag_in_frames_2_art_det);

    double ten_per_0 = lag_in_frames_0_psnr / 10;
    double ten_per_1 = lag_in_frames_1_psnr / 10;
    double ten_per_2 = lag_in_frames_2_psnr / 10;

    int lng_rc_1 = vpxt_compare_enc(lag_in_frames_0.c_str(),
        lag_in_frames_1.c_str(),0);
    int lng_rc_2 = vpxt_compare_enc(lag_in_frames_1.c_str(),
        lag_in_frames_2.c_str(),0);

    std::string quant_in_0_str;
    vpxt_remove_file_extension(lag_in_frames_0.c_str(), quant_in_0_str);
    quant_in_0_str += "quantizers.txt";
    int lag_in_frames_found_0 =vpxt_lag_in_frames_check(quant_in_0_str.c_str());

    std::string quant_in_1_str;
    vpxt_remove_file_extension(lag_in_frames_1.c_str(), quant_in_1_str);
    quant_in_1_str += "quantizers.txt";
    int lag_in_frames_found_1 =vpxt_lag_in_frames_check(quant_in_1_str.c_str());

    std::string quant_in_2_str;
    vpxt_remove_file_extension(lag_in_frames_2.c_str(), quant_in_2_str);
    quant_in_2_str += "quantizers.txt";
    int lag_in_frames_found_2 =vpxt_lag_in_frames_check(quant_in_2_str.c_str());

    char lag_in_frame_0_file_name[255] = "";
    char lag_in_frame_1_file_name[255] = "";
    char lag_in_frame_2_file_name[255] = "";

    vpxt_file_name(lag_in_frames_0.c_str(), lag_in_frame_0_file_name, 0);
    vpxt_file_name(lag_in_frames_1.c_str(), lag_in_frame_1_file_name, 0);
    vpxt_file_name(lag_in_frames_2.c_str(), lag_in_frame_2_file_name, 0);

    int test_state = kTestPassed;
    tprintf(PRINT_BTH, "\n\nResults:\n\n");

    if (lag_in_frames_found_0 == 0)
    {
        vpxt_formated_print(RESPRT, "%s properly lagged %i frames - Passed",
            lag_in_frame_0_file_name, lag_in_frames_found_0);
        tprintf(PRINT_BTH, "\n");
    }
    else
    {
        vpxt_formated_print(RESPRT, "%s improperly lagged frames %i - Failed",
            lag_in_frame_0_file_name, lag_in_frames_found_0);
        tprintf(PRINT_BTH, "\n");
    }

    if (lag_in_frames_found_1 == lag_in_frames_1_val)
    {
        vpxt_formated_print(RESPRT, "%s properly lagged %i frames - Passed",
            lag_in_frame_1_file_name, lag_in_frames_found_1);
        tprintf(PRINT_BTH, "\n");
    }
    else
    {
        vpxt_formated_print(RESPRT, "%s improperly lagged %i frames - Failed",
            lag_in_frame_1_file_name, lag_in_frames_found_1);
        tprintf(PRINT_BTH, "\n");
    }

    if (lag_in_frames_found_2 == lag_in_frames_2_val)
    {
        vpxt_formated_print(RESPRT, "%s properly lagged %i frames - Passed",
            lag_in_frame_2_file_name, lag_in_frames_found_2);
        tprintf(PRINT_BTH, "\n");
    }
    else
    {
        vpxt_formated_print(RESPRT, "%s improperly lagged %i frames - Failed",
            lag_in_frame_2_file_name, lag_in_frames_found_2);
        tprintf(PRINT_BTH, "\n");
    }

    if (lng_rc_1 == -1)
    {
        vpxt_formated_print(RESPRT, "%s identical to %s - Failed",
            lag_in_frame_0_file_name, lag_in_frame_1_file_name);
        tprintf(PRINT_BTH, "\n");
        test_state = kTestFailed;
    }
    else
    {
        vpxt_formated_print(RESPRT, "%s not identical to %s - Passed",
            lag_in_frame_0_file_name, lag_in_frame_1_file_name);
        tprintf(PRINT_BTH, "\n");
    }

    if (lng_rc_2 == -1)
    {
        vpxt_formated_print(RESPRT, "%s identical to %s - Failed",
            lag_in_frame_1_file_name, lag_in_frame_2_file_name);
        tprintf(PRINT_BTH, "\n");
        test_state = kTestFailed;
    }
    else
    {
        vpxt_formated_print(RESPRT, "%s not identical to %s - Passed",
            lag_in_frame_1_file_name, lag_in_frame_2_file_name);
        tprintf(PRINT_BTH, "\n");
    }

    if (lag_in_frames_1_psnr <= (lag_in_frames_0_psnr + ten_per_0) &&
        lag_in_frames_1_psnr >= (lag_in_frames_0_psnr - ten_per_0))
    {
        vpxt_formated_print(RESPRT, "PSNR for %s is within 10%% of PSNR for %s "
            "- %.2f < %.2f < %.2f - Passed", lag_in_frame_0_file_name,
            lag_in_frame_1_file_name, (lag_in_frames_0_psnr - ten_per_0),
            lag_in_frames_1_psnr, (lag_in_frames_0_psnr + ten_per_0));
        tprintf(PRINT_BTH, "\n");
    }
    else
    {
        if (!(lag_in_frames_1_psnr <= (lag_in_frames_0_psnr + ten_per_0)))
        {
            vpxt_formated_print(RESPRT, "PSNR for %s is not within 10%% of "
                "PSNR for %s - %.2f < %.2f - Failed", lag_in_frame_0_file_name,
                lag_in_frame_1_file_name, (lag_in_frames_0_psnr + ten_per_0),
                lag_in_frames_1_psnr);
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
        else
        {
            vpxt_formated_print(RESPRT, "PSNR for %s is not within 10%% of "
                "PSNR for %s - %.2f < %.2f - Failed", lag_in_frame_0_file_name,
                lag_in_frame_1_file_name, lag_in_frames_1_psnr,
                (lag_in_frames_0_psnr - ten_per_0));
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
    }

    if (lag_in_frames_2_psnr <= (lag_in_frames_1_psnr + ten_per_1) &&
        lag_in_frames_2_psnr >= (lag_in_frames_1_psnr - ten_per_1))
    {
        vpxt_formated_print(RESPRT, "PSNR for %s is within 10%% of PSNR for %s "
            "- %.2f < %.2f < %.2f - Passed", lag_in_frame_1_file_name,
            lag_in_frame_2_file_name, (lag_in_frames_1_psnr - ten_per_1),
            lag_in_frames_2_psnr, (lag_in_frames_1_psnr + ten_per_1));
        tprintf(PRINT_BTH, "\n");
    }
    else
    {
        if (!(lag_in_frames_1_psnr <= (lag_in_frames_0_psnr + ten_per_0)))
        {
            vpxt_formated_print(RESPRT, "PSNR for %s is not within 10%% of "
                "PSNR for %s - %.2f < %.2f - Failed", lag_in_frame_1_file_name,
                lag_in_frame_2_file_name, (lag_in_frames_1_psnr + ten_per_1),
                lag_in_frames_2_psnr);
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
        else
        {
            vpxt_formated_print(RESPRT, "PSNR for %s is not within 10%% of "
                "PSNR for %s - %.2f < %.2f - Failed", lag_in_frame_1_file_name,
                lag_in_frame_2_file_name, lag_in_frames_2_psnr,
                (lag_in_frames_1_psnr - ten_per_1));
            tprintf(PRINT_BTH, "\n");
            test_state = kTestFailed;
        }
    }

    // handle possible artifact
    if(lag_in_frames_0_art_det == kPossibleArtifactFound ||
        lag_in_frames_1_art_det == kPossibleArtifactFound ||
        lag_in_frames_2_art_det == kPossibleArtifactFound)
    {
        tprintf(PRINT_BTH, "\nPossible Artifact\n");

        fclose(fp);
        record_test_complete(file_index_str, file_index_output_char, test_type);
        return kTestPossibleArtifact;
    }

    if (test_state == kTestPassed)
        tprintf(PRINT_BTH, "\nPassed\n");
    if (test_state == kTestFailed)
        tprintf(PRINT_BTH, "\nFailed\n");

    if (delete_ivf)
        vpxt_delete_files(3, lag_in_frames_0.c_str(), lag_in_frames_1.c_str(),
        lag_in_frames_2.c_str());

    fclose(fp);
    record_test_complete(file_index_str, file_index_output_char, test_type);
    return test_state;
}