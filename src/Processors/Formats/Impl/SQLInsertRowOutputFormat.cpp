#include <Processors/Formats/Impl/SQLInsertRowOutputFormat.h>
#include <IO/WriteHelpers.h>


namespace DB
{

SQLInsertRowOutputFormat::SQLInsertRowOutputFormat(WriteBuffer & out_, const Block & header_, const RowOutputFormatParams & params_, const FormatSettings & format_settings_)
    : IRowOutputFormat(header_, out_, params_), column_names(header_.getNames()), format_settings(format_settings_)
{
}

void SQLInsertRowOutputFormat::writeRowStartDelimiter()
{
    if (rows_in_line == 0)
    {
        if (format_settings.sql_insert.use_replace)
            writeCString("REPLACE INTO ", out);
        else
            writeCString("INSERT INTO ", out);
        writeString(format_settings.sql_insert.table_name, out);
        if (format_settings.sql_insert.include_column_names)
        {
            writeCString(" (", out);
            for (size_t i = 0; i != column_names.size(); ++i)
            {
                writeString(column_names[i], out);
                if (i + 1 != column_names.size())
                    writeCString(", ", out);
            }
            writeChar(')', out);
        }
        writeCString(" VALUES ", out);
    }
    writeChar('(', out);
}

void SQLInsertRowOutputFormat::writeField(const IColumn & column, const ISerialization & serialization, size_t row_num)
{
    serialization.serializeTextQuoted(column, row_num, out, format_settings);
}

void SQLInsertRowOutputFormat::writeFieldDelimiter()
{
    writeCString(", ", out);
}

void SQLInsertRowOutputFormat::writeRowEndDelimiter()
{
    writeChar(')', out);
    ++rows_in_line;
}

void SQLInsertRowOutputFormat::writeRowBetweenDelimiter()
{
    if (rows_in_line >= format_settings.sql_insert.max_batch_size)
    {
        writeCString(";\n", out);
        rows_in_line = 0;
    }
    else
    {
        writeCString(", ", out);
    }
}

void SQLInsertRowOutputFormat::writeSuffix()
{
    writeCString(";\n", out);
}


void registerOutputFormatSQLInsert(FormatFactory & factory)
{
    factory.registerOutputFormat("SQLInsert", [](
        WriteBuffer & buf,
        const Block & sample,
        const RowOutputFormatParams & params,
        const FormatSettings & settings)
    {
        return std::make_shared<SQLInsertRowOutputFormat>(buf, sample, params, settings);
    });
}


}
