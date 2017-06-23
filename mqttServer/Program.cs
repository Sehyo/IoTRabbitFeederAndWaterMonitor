using System;
using System.Text;
using uPLibrary.Networking.M2Mqtt; // (Nugget Packet: "M2Mqtt")
using uPLibrary.Networking.M2Mqtt.Messages;
using System.Text.RegularExpressions;
namespace MQTTClientIOT
{
    class Program
    {
        static MqttClient client = new MqttClient("192.168.1.11");
        static SQLStuff sqlClient = new SQLStuff();
        static void Main(string[] args)
        {
            System.Console.WriteLine(getTime());
            // Testing SQL Stuff
            sqlClient.connect();
            // Testing ends here
            String clientId = "CLIENTIDREMOVED";
            ushort msgId = client.Subscribe(new string[] { Topics.foodRefillTopic, Topics.waterTopic}, new byte[] {MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE, MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE});
            client.MqttMsgPublished += client_MqttMsgPublished;
            client.MqttMsgPublishReceived += client_MqttMsgPublishReceived;
            client.MqttMsgSubscribed += client_MqttMsgSubscribed;
            // Default port of this class is 1883 :)
            //byte result = client.Connect(Guid.NewGuid().ToString());
            byte result = client.Connect(clientId);
            System.Console.WriteLine("Connection Result: {0}", result);
            while(true) // Program Loop
            {
                System.Console.WriteLine("Iterating");
                if(timeToFeed()) publishTimeToFeed();
                System.Threading.Thread.Sleep(5000);
            }
            System.Console.WriteLine("Program Finished");
            System.Console.ReadLine();
        }

        public static double getTime()
        {
            return Double.Parse(DateTime.Now.ToString().Replace(":", "").Replace("-", "").Replace(" ", ""));
        }

        public static void client_MqttMsgPublishReceived(object sender, MqttMsgPublishEventArgs e)
        {
            switch (e.Topic)
            {
                case Topics.foodRefillTopic:
                    //updateDBFoodRefill(); // This was implemented in the webpanel instead..
                    break;
                case Topics.waterTopic:
                    Regex reg = new Regex(@"[0-9]+\.[0-9]+");
                    Match m = reg.Match(Encoding.UTF8.GetString(e.Message));
                    float waterLevel = float.Parse(m.Value.Replace('.', ','));
                    System.Console.WriteLine("Water Level: " + waterLevel);
                    if(waterLevel <= 0.01)  sendWaterWarningMail();
                    sqlClient.execute(string.Format(sqlClient.queryAddWaterLevel, getTime(), waterLevel));
                    break;
            }
        }

        public static void sendWaterWarningMail()
        {
			// Fix implementation later
            // Send an email to whatever email specified containing the message string.
         //   using (System.Net.WebClient client = new System.Net.WebClient())
                //client.DownloadString(@"localhost/IOTASS5/waterEmail.php"); // uhhh not sure how to do this for localhost
        }

        public static void client_MqttMsgPublished(object sender, MqttMsgPublishedEventArgs e)
        {
            System.Console.WriteLine("Message {0}, published == {1}", e.MessageId, e.IsPublished);
        }

        public static void client_MqttMsgSubscribed(object sender, MqttMsgSubscribedEventArgs e)
        {
            System.Console.WriteLine("Subscribed to ID: {0}", e.MessageId);
        }

        static bool timeToFeed()
        {
            if (sqlClient.retrieveColumn("isItTimeToFeed").Equals("1")) return true;
            else return false;
        }

        static void publishTimeToFeed()
        {
            // 1 signifies feeding time..
            ushort msgId = client.Publish(Topics.timeToFeedTopic, Encoding.UTF8.GetBytes("1"), MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE, false);
            System.Console.WriteLine("Published Message: {0}", msgId);
            sqlClient.execute(sqlClient.queryResetTimeToFeed);
            sqlClient.execute(sqlClient.queryReducePortionsRemaining);
            sqlClient.execute(sqlClient.queryIncrementTotalTimesFed);
        }
    }
}