## CSV Completeness Checker

A fast and easy-to-use tool that analyzes CSV files for data completeness by checking for missing/invalid values across different column combinations.

### Features

  * Checks for missing or invalid values across user defined column combinations.
  * Saves and loads analysis configurations to and from JSON files for easy reuse.
  * Interactive command line interface to guide you through the analysis setup.
  * Full command line automation support for scripting and batch processing.

### Build

Just let CMake do its magic.

***Dependencies** (automatically fetched):*

> * fast-cpp-csv-parser
> * nlohmann_json
> * cxxopts
> * fmt

### Usage

#### Interactive Mode

Run the application without arguments to use the interactive mode:

```
./csv-completeness-checker
```

The application will guide you through the following steps:

1. **Provide a CSV or JSON file:**

      * You can start with a `.csv` file to analyze.
      * Or, you can provide a `.json` file with a saved configuration from a previous session.

2. **Select Fields to Analyze:**

      * Choose the columns from your CSV file that you want to include in the analysis.

3. **Define Invalid Values:**

      * For each selected field, you can specify a comma-separated list of values that should be considered invalid or empty (e.g., `N/A`, `Unspecified`, `-1`).

4. **Define Column Combinations:**

      * Specify the combinations of columns you want to check for completeness. Use `:` to require multiple fields together and `/` inside a group to indicate alternatives. For example:
        * `1:3` checks rows where both column 1 and column 3 are valid.
        * `1:2:3/4` checks rows where columns 1 and 2 are valid and at least one of columns 3 or 4 is valid.
      * You can also provide multiple combinations separated by commas (e.g., `1:2, 1:2:3/4`).

5. **Save Configuration (Optional):**

      * You can save your configuration (selected fields, invalid values, and combinations) to a JSON file for later use.

6. **Process the CSV:**

      * The tool will then process the CSV file and output the results for each column combination, showing both the raw counts (valid rows / total rows) and the completeness percentage.

#### Command Line Mode

Use command line arguments to automate the analysis without interactive prompts:

```
./csv-completeness-checker [OPTIONS]
```

**Options:**

| Argument | Description |
|-|-|
| `--csv, -c` | Path to CSV file to analyze |
| `--config, -C` | Path to JSON configuration file (overrides --csv) |
| `--output, -o` | Path to save output JSON configuration |
| `--fields, -f` | Comma-separated field numbers to analyze |
| `--invalid-values, -i` | Invalid values mapping (format: `field:value1,value2:field:value3...`) |
| `--combinations, -b` | Column combinations to check (format: `1:2,1:3/4`) |
| `--silent, -q` | Minimal output (only results) |
| `--help, -h` | Display help message |