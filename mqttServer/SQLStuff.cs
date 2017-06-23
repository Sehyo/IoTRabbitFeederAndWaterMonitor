using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using MySql.Data; // (Nugget Packet: "MySql.Data")
using MySql.Data.MySqlClient;

namespace MQTTClientIOT
{
    class SQLStuff
    {
        public string user = "USERNAMEREMOVED";
        public string dbName = "DBNAMEREMOVED";
        public string password = "PASSWORDREMOVED";
        public string queryReducePortionsRemaining = "UPDATE feeder SET portionsRemaining = portionsRemaining - 1 WHERE feederID = 1 and portionsRemaining > 0";
        public string queryIncrementTotalTimesFed = "UPDATE feeder SET totalTimesFed = totalTimesFed + 1 WHERE feederID = 1";
        public string queryAddWaterLevel = "INSERT INTO waterstamps (feederID, measureStamp, measurement) VALUES (1, {0}, {1})";
        public string queryRetrieveColumn = "SELECT {0} FROM feeder WHERE feederID = 1";
        public string queryResetTimeToFeed = "UPDATE feeder set isItTimeToFeed = 0";
        public MySqlConnection sqlConnection = null;
        
        public int connect()
        {
            if (sqlConnection != null) return 1;
            sqlConnection = new MySqlConnection(string.Format("Server=localhost; database={0}; UID={1}; password={2}", dbName, user, password));
            sqlConnection.Open();
            return 0;
        }

        public int execute(string executee)
        {
            if (sqlConnection == null) connect();
            var cmd = new MySqlCommand(executee, sqlConnection).ExecuteReader();
            cmd.Close();
            return 0;
        }

        public string retrieveColumn(string executee)
        {
            if (sqlConnection == null) return null;
            string query = string.Format(queryRetrieveColumn, executee);
            var reader = new MySqlCommand(query, sqlConnection).ExecuteReader();
            reader.Read();
            query = reader.GetString(0);
            reader.Close();
            return query;
        }
    }
}